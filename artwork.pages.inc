<?php

/**
 * Menu callback; Show list of artwork types we can add.
 */
function artwork_add_page() {
  $item = menu_get_item();
  $links = system_admin_menu_block($item);

  foreach ($links as $link) {
    $items[] = l($link['title'], $link['href'], $item['localized_options'])
      . ': ' . filter_xss_admin($link['description']);
  }

  return theme('item_list', array('items' => $items));
}

/**
 * Present an artwork submission form.
 */
function artwork_add($type) {
  global $user;

  $types = artwork_types();
  $type = isset($type) ? str_replace('-', '_', $type) : NULL;
  if (empty($types[$type])) {
    return MENU_NOT_FOUND;
  }

  $artwork = entity_get_controller('artwork')->create($type);

  drupal_set_title(t('Create @name', array('@name' => $types[$type]->name)), PASS_THROUGH);
  return drupal_get_form($type . '_artwork_form', $artwork);
}

/**
 * Menu callback; presents the artwork editing form, or redirects to delete confirmation.
 *
 * @param $artwork
 *   The artwork object to edit.
 */
function artwork_page_edit($artwork) {
  $types = artwork_types();
  drupal_set_title(t('<em>Edit @type</em> @title', array('@type' => $types[$artwork->type]->name, '@title' => $artwork->title)), PASS_THROUGH);

  return drupal_get_form($artwork->type . '_artwork_form', $artwork);
}

/**
 * Form builder; Displays the artwork add/edit form.
 *
 * @param $form
 * @param $form_state
 * @param $artwork
 *   The artwork object to edit, which may be brand new.
 */
function artwork_form($form, &$form_state, $artwork) {

  // Set the id and identify this as an artwork edit form.
  $form['#id'] = 'artwork-form';

  // Save the artwork for later, in case we need it.
  $form['#artwork'] = $artwork;
  $form_state['artwork'] = $artwork;

  // Common fields. We don't have many.
  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#default_value' => $artwork->title,
    '#weight' => -5,
    '#required' => TRUE,
  );

  $form['revision'] = array(
    '#access' => user_access('administer artworks'),
    '#type' => 'checkbox',
    '#title' => t('Create new revision'),
    '#default_value' => 0,
  );

  // Add the buttons.
  $form['buttons'] = array();
  $form['buttons']['#weight'] = 100;
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 5,
    '#submit' => array('artwork_form_submit'),
  );
  if (!empty($artwork->aid)) {
    $form['buttons']['delete'] = array(
      '#access' => user_access('delete artworks'),
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#weight' => 15,
      '#submit' => array('artwork_form_delete_submit'),
    );
  }

  $form['#validate'][] = 'artwork_form_validate';

  field_attach_form('artwork', $artwork, $form, $form_state);

  return $form;
}

function artwork_form_validate($form, &$form_state) {
  $artwork = $form_state['artwork'];

  // Field validation.
  field_attach_form_validate('artwork', $artwork, $form, $form_state);
}


function artwork_form_submit($form, &$form_state) {
  global $user;

  $artwork = &$form_state['artwork'];

  // Set the artwork's uid if it's being created at this time.
  if (empty($artwork->uid)) {
    $artwork->uid = $user->uid;
  }

  $artwork->title = $form_state['values']['title'];
  $artwork->revision = $form_state['values']['revision'];

  // Notify field widgets.
  field_attach_submit('artwork', $artwork, $form, $form_state);

  // Save the artwork.
  artwork_save($artwork);

  // Notify the user.
  drupal_set_message(t('Artwork saved.'));

  $form_state['redirect'] = 'artwork/' . $artwork->aid;
}


function artwork_form_delete_submit($form, &$form_state) {
  $destination = array();
  if (isset($_GET['destination'])) {
    $destination = drupal_get_destination();
    unset($_GET['destination']);
  }
  $artwork = $form['#artwork'];
  $form_state['redirect'] = array('artwork/' . $artwork->aid . '/delete', array('query' => $destination));
}

/**
 * Displays an artwork.
 *
 * @param $artwork
 *   The artwork object to display.
 * @param $view_mode
 *   The view mode we want to display.
 */
function artwork_page_view($artwork, $view_mode = 'full') {
  // Remove previously built content, if exists.
  $artwork->content = array();

  if ($view_mode == 'teaser') {
    $artwork->content['title'] = array(
      '#markup' => filter_xss($artwork->title),
      '#weight' => -5,
    );
  }

  // Build fields content.
  field_attach_prepare_view('artwork', array($artwork->aid => $artwork), $view_mode);
  entity_prepare_view('artwork', array($artwork->aid => $artwork));
  $artwork->content += field_attach_view('artwork', $artwork, $view_mode);
  
  return $artwork->content;
}

/**
 * Form bulder; Asks for confirmation of artwork deletion.
 */
function artwork_delete_confirm($form, &$form_state, $artwork) {
  $form['#artwork'] = $artwork;
  // Always provide entity id in the same form key as in the entity edit form.
  $form['aid'] = array('#type' => 'value', '#value' => $artwork->aid);
  return confirm_form($form,
    t('Are you sure you want to delete %title?', array('%title' => $artwork->title)),
    'artwork/' . $artwork->aid,
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Executes artwork deletion.
 */
function artwork_delete_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    $artwork = artwork_load($form_state['values']['aid']);
    artwork_delete($form_state['values']['aid']);
    watchdog('artwork', '@type: deleted %title.', array('@type' => $artwork->type, '%title' => $artwork->title));

    $types = artwork_types();
    drupal_set_message(t('@type %title has been deleted.', array('@type' => $types[$artwork->type]->name, '%title' => $artwork->title)));
  }

  $form_state['redirect'] = '<front>';
}

/**
 * Menu callback; Displays a listing of recent artworks.
 *
 * This doesn't really work yet because our presentation code doesn't show
 * the title.
 */
function artwork_page_list_recent() {
  $content = array();

  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'artwork')
    ->propertyOrderBy('created', 'DESC')
    ->fieldCondition('field_artist', 'value', 'Da Vinci', 'CONTAINS', 0)
    ->range(0, 5);
  $result = $query->execute();

  $artworks = artwork_load_multiple(array_keys($result['artwork']));
  foreach ($artworks as $artwork) {
    $content[$artwork->aid] = artwork_page_view($artwork, 'teaser');
  }

  return $content;
}
