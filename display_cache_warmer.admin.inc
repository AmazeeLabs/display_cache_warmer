<?php

/**
 * Settings form callback.
 */
function display_cache_warmer_settings_form($form, &$form_state) {
  $form['display_cache_warmer_max_count_per_cron_run'] = array(
    '#type' => 'textfield',
    '#required' => TRUE,
    '#title' => t('Max count per cron run'),
    '#description' => 'The maximum number of caches that can be successfully warmed per one cron run. Zero means no limit.',
    '#default_value' => _display_cache_warmer_variable_get('max_count_per_cron_run'),
  );
  $form['display_cache_warmer_max_age'] = array(
    '#type' => 'textfield',
    '#required' => TRUE,
    '#title' => t('Max age'),
    '#description' => 'The maximum age in seconds of the log record. Default is 30 days (2592000 seconds).',
    '#default_value' => _display_cache_warmer_variable_get('max_age'),
  );
  $form['display_cache_warmer_wipe_out'] = array(
    '#type' => 'checkbox',
    '#title' => t('Wipe out'),
    '#description' => 'Check if you want to delete old log records. Leave unchecked to just skip them.',
    '#default_value' => _display_cache_warmer_variable_get('wipe_out'),
  );
  $form['display_cache_warmer_debug_mode'] = array(
    '#type' => 'checkbox',
    '#title' => t('Debug mode'),
    '#description' => 'Normally, the watchdog() is only called when there is at least one successfully warmed cache. Enable this option to call the watchdog() on each cron run.',
    '#default_value' => _display_cache_warmer_variable_get('debug_mode'),
  );
  return system_settings_form($form);
}

/**
 * Validation handler for the settings form.
 */
function display_cache_warmer_settings_form_validate(&$form, &$form_state) {
  $numeric = array(
    'display_cache_warmer_max_count_per_cron_run',
    'display_cache_warmer_max_age',
  );
  foreach ($numeric as $name) {
    if (!ctype_digit($form_state['values'][$name])) {
      form_error($form[$name], t('Only numbers are allowed in %field.', array(
        '%field' => $form[$name]['#title'],
      )));
    }
  }
}
