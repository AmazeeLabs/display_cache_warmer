<?php

/**
 * Settings form callback.
 */
function display_cache_warmer_settings_form($form, &$form_state) {
  $form['display_cache_warmer_max_age'] = array(
    '#type' => 'textfield',
    '#required' => TRUE,
    '#title' => t('Max age'),
    '#description' => 'The maximum age in seconds of the log record. Default is 30 days (2592000 seconds).',
    '#default_value' => _display_cache_warmer_variable_get('max_age'),
  );
  $form['display_cache_warmer_wipe_out'] = array(
    '#type' => 'checkbox',
    '#required' => TRUE,
    '#title' => t('Wipe out'),
    '#description' => 'Check if you want to delete old log records. Leave unchecked to just skip them.',
    '#default_value' => _display_cache_warmer_variable_get('wipe_out'),
  );
  return system_settings_form($form);
}

/**
 * Validation handler for the settings form.
 */
function display_cache_warmer_settings_form_validate(&$form, &$form_state) {
  if (!ctype_digit($form_state['values']['display_cache_warmer_max_age'])) {
    form_error($form['display_cache_warmer_max_age'], t('Only numbers are allowed in %field.', array(
      '%field' => $form['display_cache_warmer_max_age']['#title'],
    )));
  }
}
