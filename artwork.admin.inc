<?php

/**
 * Menu callback; List all artwork types available.
 */
function artwork_overview_types() {
  foreach (artwork_types() as $type => $info) {
    $type_url_str = str_replace('_', '-', $type);
    $label = t('View @type', array('@type' => $info->name));
    $items[] = l($label, 'admin/structure/artworks/manage/' . $type_url_str);
  }

  return theme('item_list', array('items' => $items));
}

/**
 * Menu callback; Artwork information page.
 *
 * @param object $artwork_type
 */
function artwork_information($artwork_type) {
  return $artwork_type->name . ': ' . $artwork_type->description;
}
