<?php

/**
 * Controller for loading, creating, and saving artworks.
 *
 * The default loader, which we extend, handles load() already. We only
 * need to add saving and creating.
 */
class ArtworkController extends DrupalDefaultEntityController {

  public function save($artwork) {
    $transaction = db_transaction();

    try {
      global $user;

      // Determine if we will be inserting a new artwork.
      $artwork->is_new = empty($artwork->aid);

      // Set the timestamp fields.
      if (empty($artwork->created)) {
        $artwork->created = REQUEST_TIME;
      }
      $artwork->changed = REQUEST_TIME;

      $artwork->revision_timestamp = REQUEST_TIME;
      $update_artwork = TRUE;

      // Give modules the opportunity to prepare field data for saving.
      field_attach_presave('artwork', $artwork);

      // When saving a new artwork revision, unset any existing $artwork->vid
      // to ensure a new revision will actually be created and store the old
      // revision ID in a separate property for artwork hook implementations.
      if (!$artwork->is_new && !empty($artwork->revision) && $artwork->vid) {
        $artwork->old_vid = $artwork->vid;
        unset($artwork->vid);
      }

      // If this is a new artwork...
      if ($artwork->is_new) {
        // Save the new artwork.
        drupal_write_record('artwork', $artwork);

        // Save the initial revision.
        $this->saveRevision($artwork, $user->uid);

        $op = 'insert';
      }
      else {
        // Save the updated artwork.
        drupal_write_record('artwork', $artwork, 'aid');

        // If a new artwork revision was requested, save a new record for that;
        // otherwise, update the artwork revision record that matches the value
        // of $artwork->vid.
        if (!empty($artwork->revision)) {
          $this->saveRevision($artwork, $user->uid);
        }
        else {
          $this->saveRevision($artwork, $user->uid, TRUE);
          $update_artwork = FALSE;
        }

        $op = 'update';
      }

      // If the revision ID is new or updated, save it to the artwork.
      if ($update_artwork) {
        db_update('artwork')
          ->fields(array('vid' => $artwork->vid))
          ->condition('aid', $artwork->aid)
          ->execute();
      }

      // Save fields.
      $function = 'field_attach_' . $op;
      $function('artwork', $artwork);

      module_invoke_all('entity_' . $op, $artwork, 'artwork');

      // Clear internal properties.
      unset($artwork->is_new);

      // Ignore slave server temporarily to give time for the saved order to be
      // propagated to the slave.
      db_ignore_slave();

      return $artwork;
    }
    catch (Exception $e) {
      $transaction->rollback();
      watchdog_exception('artwork', $e, NULL, WATCHDOG_ERROR);
      return FALSE;
    }
  }

  /**
   * Saves an artwork revision with the uid of the current user.
   *
   * @param $artwork
   *   The fully loaded artwork object.
   * @param $uid
   *   The user's uid for the current revision.
   * @param $update
   *   TRUE or FALSE indicating whether or not the existing revision should be
   *     updated instead of a new one created.
   */
  function saveRevision($artwork, $uid, $update = FALSE) {
    // Hold on to the artwork's original creator_uid but swap in the revision's
    // creator_uid for the momentary write.
    $temp_uid = $artwork->uid;
    $artwork->uid = $uid;

    // Update the existing revision if specified.
    if ($update) {
      drupal_write_record('artwork_revision', $artwork, 'vid');
    }
    else {
      // Otherwise insert a new revision. This will automatically update $artwork
      // to include the vid.
      drupal_write_record('artwork_revision', $artwork);
    }

    // Reset the order's creator_uid to the original value.
    $artwork->uid = $temp_uid;
 }

  /**
   * Deletes multiple artworks by ID.
   *
   * @param $aids
   *   An array of artwork IDs to delete.
   * @return
   *   TRUE on success, FALSE otherwise.
   */
  public function delete($aids) {
    if (!empty($aids)) {
      $artworks = $this->load($aids, array());

      $transaction = db_transaction();

      try {
        db_delete('artwork')
          ->condition('aid', $aids, 'IN')
          ->execute();

        db_delete('artwork_revision')
          ->condition('aid', $aids, 'IN')
          ->execute();

        foreach ($artworks as $artwork_id => $artwork) {
          field_attach_delete('artwork', $artwork);
        }

        // Ignore slave server temporarily to give time for the
        // saved artwork to be propagated to the slave.
        db_ignore_slave();
      }
      catch (Exception $e) {
        $transaction->rollback();
        watchdog_exception('artwork', $e, NULL, WATCHDOG_ERROR);
        return FALSE;
      }

      module_invoke_all('entity_delete', $artwork, 'artwork');

      // Clear the page and block and artwork caches.
      cache_clear_all();
      $this->resetCache();
    }

    return TRUE;
  }

  /**
   * Create a default artwork.
   *
   * @param $type
   *   The machine-readable type of the artwork.
   *
   * @return
   *   An artwork object with all default fields initialized.
   */
  public function create($type = '') {
    return (object) array(
      'aid' => '',
      'type' => $type,
      'title' => '',
    );
  }
}
