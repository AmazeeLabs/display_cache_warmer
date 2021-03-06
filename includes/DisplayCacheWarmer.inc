<?php

/**
 * Cache warmer.
 *
 * Only two methods could be called outside:
 * @see DisplayCacheWarmer::warmCaches()
 * @see DisplayCacheWarmer::warmCache()
 */
class DisplayCacheWarmer {

  /**
   * Cache ID.
   *
   * @var string
   */
  protected $cacheId;

  /**
   * Cache specification object.
   *
   * @var DisplayCacheWarmerCacheSpec
   */
  protected $cacheSpec;

  /**
   * Constructor.
   *
   * @param string $cache_id
   *   Cache ID.
   */
  protected function __construct($cache_id) {
    $this->cacheId = $cache_id;
  }

  /**
   * Warms caches.
   *
   * @param int $max_count
   *   The maximum number of caches that can be successfully warmed. Zero means
   *   no limit.
   */
  public static function warmCaches($max_count = 0) {
    // We'll release the lock in the _display_cache_warmer_log().
    if (!lock_acquire('display_cache_warmer', 60*60*24)) {
      return;
    }
    register_shutdown_function('_display_cache_warmer_log');
    $log =& display_cache_warmer_static();
    timer_start('display_cache_warmer');
    $log['success'] = 0;
    $log['exceptions'] = array();
    $reader = new \OSInet\Heisencache\CacheReadLogReader();
    $max_age = (int) _display_cache_warmer_variable_get('max_age');
    $wipe_out = (bool) _display_cache_warmer_variable_get('wipe_out');
    $log['debug_mode'] = (bool) _display_cache_warmer_variable_get('debug_mode');
    $reader->query('cache_display_cache', $max_age, $wipe_out);
    while ((($cache_id = $reader->getNextCacheId(TRUE)) !== FALSE) && ($log['success'] < $max_count)) {
      try {
        self::warmCache($cache_id);
        $log['success']++;
      }
      catch (DisplayCacheWarmerException $e) {
        $log['exceptions'][] = $e;
      }
      catch (Exception $e) {
        $log['exceptions'][] = new DisplayCacheWarmerException('An exception was caught during the cache warming: ' . $e->getMessage(), $cache_id);
      }
    }
    $log['completed'] = TRUE;
    _display_cache_warmer_log();
  }

  /**
   * Warms a specific cache.
   *
   * @param string $cache_id
   *   Cache ID.
   *
   * @throws DisplayCacheWarmerException
   * @throws Exception
   */
  public static function warmCache($cache_id) {
    $log_writer_state = \OSInet\Heisencache\CacheReadLogWriterSubscriber::getState();
    \OSInet\Heisencache\CacheReadLogWriterSubscriber::setState(FALSE);
    $cache_warmer = new self($cache_id);
    try {
      $cache_warmer
        ->parseCacheId()
        ->validateCacheSpec()
        ->proxy()
        ->buildCache()
        ->proxy(TRUE);
    }
    catch (Exception $e) {
      $cache_warmer->proxy(TRUE);
      \OSInet\Heisencache\CacheReadLogWriterSubscriber::setState($log_writer_state);
      throw $e;
    }
    \OSInet\Heisencache\CacheReadLogWriterSubscriber::setState($log_writer_state);
  }

  /**
   * Parses cache ID.
   *
   * Cache ID specification:
   * | cache keys:                           | altered cache keys:         | granularity:                                  |
   *  entity_type:entity_id:view_mode:element:domain_id?:domain_variant_id?:theme_name:langcode*:(user_role+|user_id)?:url?
   *
   * Examples:
   * node:10312:short_teaser:entity:d.1:dv.1:cool_theme:en-US:en-US:r.2,3
   * node:8:page_manager:entity:d.5:cool_theme:de:u.1:http://example.com/de/page?param=value
   *
   * Generated by:
   * @see display_cache_get_cache_keys()
   * @see hook_display_cache_cache_keys_alter()
   * @see drupal_render_cid_create()
   * @see drupal_render_cid_parts()
   *
   * @return $this
   *
   * @throws DisplayCacheWarmerException
   */
  protected function parseCacheId() {
    $pattern = '~';
    // Entity type.
    $pattern .= '^([^:]+)';
    // Entity ID.
    $pattern .= '(:([0-9]+))';
    // View mode.
    $pattern .= '(:([^:]+))';
    // Element ("entity" or field name).
    $pattern .= "(:([^:]+))";
    // Domain ID.
    $pattern .= '(:d.([0-9]+))?';
    // Domain variant ID.
    $pattern .= '(:dv.([0-9]+))?';
    // Theme name.
    $pattern .= "(:([^:]+))";
    // Languages.
    $pattern .= '((:[a-zA-Z_-]+)*)';
    // User roles.
    $pattern .= '(:r.([0-9]+(,[0-9]+)*))?';
    // User ID.
    $pattern .= '(:u.([0-9]+))?';
    // URL.
    $pattern .= '(:(https?://.*))?$';
    $pattern .= '~';
    if (!preg_match($pattern, $this->cacheId, $matches)) {
      throw new DisplayCacheWarmerException('The cache ID does not match the pattern.', $this->cacheId);
    }
    $cache_spec = new DisplayCacheWarmerCacheSpec();
    $cache_spec->cacheId = $this->cacheId;
    @list(
      ,
      $cache_spec->entityType,
      ,
      $cache_spec->entityId,
      ,
      $cache_spec->viewMode,
      ,
      $cache_spec->element,
      ,
      $cache_spec->domainId,
      ,
      $cache_spec->domainVariantId,
      ,
      $cache_spec->themeName,
      $cache_spec->langcodes,
      ,
      ,
      $cache_spec->userRoleIds,
      ,
      ,
      $cache_spec->userId,
      ,
      $cache_spec->url
    ) = $matches;
    $cache_spec->userRoleIds = $cache_spec->userRoleIds ? drupal_map_assoc(explode(',', $cache_spec->userRoleIds)) : array();
    asort($cache_spec->userRoleIds);
    $cache_spec->langcodes = $cache_spec->langcodes ? explode(':', ltrim($cache_spec->langcodes, ':')) : array();
    $this->cacheSpec = $cache_spec;
    return $this;
  }

  /**
   * Validates cache specification.
   *
   * @return $this
   *
   * @throws DisplayCacheWarmerException
   */
  protected function validateCacheSpec() {
    $entity_info = entity_get_info();
    if (!isset($entity_info[$this->cacheSpec->entityType])) {
      throw new DisplayCacheWarmerException('Entity type is not registered in the system.', $this->cacheSpec);
    }
    if ($this->cacheSpec->viewMode != 'page_manager' && !isset($entity_info[$this->cacheSpec->entityType]['view modes'][$this->cacheSpec->viewMode])) {
      throw new DisplayCacheWarmerException('Unknown view mode.', $this->cacheSpec);
    }
    if ($this->cacheSpec->element != 'entity') {
      throw new DisplayCacheWarmerException('Cannot warm caches for the non entity elements.', $this->cacheSpec);
      // Do we need to be able to do this?
    }
    if ($this->cacheSpec->themeName != $GLOBALS['theme']) {
      throw new DisplayCacheWarmerException('Cannot change theme.', $this->cacheSpec);
    }
    $langcodes = array_keys(language_list());
    if ($unknown_langcodes = array_diff($this->cacheSpec->langcodes, $langcodes)) {
      throw new DisplayCacheWarmerException('Unknown langcodes: ' . implode(', ', $unknown_langcodes), $this->cacheSpec);
    }
    if (count($this->cacheSpec->langcodes) != count(language_types_configurable())) {
      throw new DisplayCacheWarmerException('Incorrect count of the langcodes: ' . count($this->cacheSpec->langcodes) . '. SHould be :' . count(language_types_configurable()), $this->cacheSpec);
    }
    if ($this->cacheSpec->userRoleIds && $this->cacheSpec->userId) {
      throw new DisplayCacheWarmerException('Both user ID and user roles IDs are provided.', $this->cacheSpec);
    }
    if ($this->cacheSpec->url) {
      throw new DisplayCacheWarmerException('Cannot warm cache by URL.', $this->cacheSpec);
    }
    return $this;
  }

  /**
   * Proxies global variables.
   *
   * @param bool $revert
   *   Whether to revert globals to initial state.
   *
   * @return $this
   *
   * @throws DisplayCacheWarmerException
   */
  protected function proxy($revert = FALSE) {

    // Reasons for resetting the static caches:
    // 1. Globals can leave some traces in the static caches.
    // 2. Some static caches needs to be cleared for sure, especially the
    //    "drupal_html_id" one, which can break forms functionality. But we
    //    don't know 100% of such cases, so clear them all.
    // 3. Some memory will be freed, which is always good.
    //
    // Reasons to not do it:
    // 1. As tested on FischerConnectors, this slows down the execution 3 times.
    //
    // So we do the reset.
    drupal_static_reset();

    // Revert globals if requested.
    static $globals_original;
    if ($revert && isset($globals_original)) {
      foreach ($globals_original as $global_name => $globals) {
        foreach ($globals as $name => $data) {
          if ($data['unset']) {
            unset($GLOBALS[$global_name][$name]);
          }
          else {
            $GLOBALS[$global_name][$name] = $data['value'];
          }
        }
      }
      if (module_exists('background_process')) {
        if ($handle = background_process_current_handle()) {
          if ($process = background_process_get_process($handle)) {
            $process->uid = $globals_original['GLOBALS']['user']['value']->uid;
            background_process_set_process($process->handle, $process->callback, $process->uid, $process->args, $process->token);
          }
        }
      }
      return $this;
    }

    // Initialize revert data.
    if (!isset($globals_original)) {
      $globals_original = array(
        'GLOBALS' => array(
          'user' => 0,
          '_domain' => 0,
          '_domain_variant' => 0,
        ) + array_flip(language_types_configurable()),
        '_SERVER' => array(
          'REQUEST_METHOD' => 0,
        ),
      );
      foreach ($globals_original as $global_name => $globals) {
        foreach ($globals as $name => $_) {
          $globals_original[$global_name][$name] = array(
            'value' => isset($GLOBALS[$global_name][$name]) ? $GLOBALS[$global_name][$name] : NULL,
            'unset' => !array_key_exists($name, $GLOBALS[$global_name]),
          );
        }
      }
    }

    // Proxy user.
    $account = NULL;
    if (!empty($this->cacheSpec->userId)) {
      $account = user_load($this->cacheSpec->userId);
      if (!$account) {
        throw new DisplayCacheWarmerException('Cannot load user for proxy.', $this->cacheSpec);
      }
    }
    elseif (!empty($this->cacheSpec->userRoleIds)) {
      if (isset($this->cacheSpec->userRoleIds[DRUPAL_ANONYMOUS_RID])) {
        $account = drupal_anonymous_user();
      }
      else {
        static $user_id_to_role_ids_map;
        if (!isset($user_id_to_role_ids_map)) {
          $user_id_to_role_ids_map = array();
          $query = db_select('users_roles', 'ur')->fields('ur');
          foreach ($query->execute() as $row) {
            $user_id_to_role_ids_map[$row->uid][$row->rid] = $row->rid;
          }
          array_walk($user_id_to_role_ids_map, 'asort');
        }
        $role_ids = $this->cacheSpec->userRoleIds;
        unset($role_ids[DRUPAL_AUTHENTICATED_RID]);
        $user_id = array_search($role_ids, $user_id_to_role_ids_map);
        if ($user_id === FALSE) {
          throw new DisplayCacheWarmerException('Cannot find a user with appropriate roles.', $this->cacheSpec);
        }
        $account = user_load($user_id);
        if (!$account) {
          throw new DisplayCacheWarmerException('Cannot load user.', $this->cacheSpec);
        }
      }
    }
    if (!empty($account)) {
      $GLOBALS['user'] = $account;
      if (module_exists('background_process')) {
        // The background_process module can change the user. It could be done
        // when some code calls menu_get_item(), and then the access callback is
        // called and the global user object is replaced.
        /* @see background_process_service_access() */
        // To avoid this, temporary replace the uid of the process.
        if ($handle = background_process_current_handle()) {
          if ($process = background_process_get_process($handle)) {
            $process->uid = $account->uid;
            background_process_set_process($process->handle, $process->callback, $process->uid, $process->args, $process->token);
          }
        }
      }
    }

    // Proxy domain and domain variant.
    if (!empty($this->cacheSpec->domainId)) {
      if (!function_exists('domain_load')) {
        throw new DisplayCacheWarmerException('The domain module is not enabled.', $this->cacheSpec);
      }
      $domain = domain_load($this->cacheSpec->domainId);
      if (!$domain) {
        throw new DisplayCacheWarmerException('Cannot load domain.', $this->cacheSpec);
      }
      $GLOBALS['_domain'] = $domain;
    }
    if (!empty($this->cacheSpec->domainVariantId)) {
      if (!function_exists('domain_variants_load')) {
        throw new DisplayCacheWarmerException('The domain_variants module is not enabled.', $this->cacheSpec);
      }
      $domain_variant = domain_variants_load($this->cacheSpec->domainVariantId);
      if (!$domain_variant) {
        throw new DisplayCacheWarmerException('Cannot load domain variant.', $this->cacheSpec);
      }
      $GLOBALS['_domain_variant'] = $domain_variant;
    }

    // Proxy languages.
    // The langcodes from the cache spec should go in the same order as it
    // returned by language_types_configurable().
    /* @see drupal_render_cid_parts() */
    $languages = language_list();
    $langcodes = $this->cacheSpec->langcodes;
    foreach (language_types_configurable() as $language_type) {
      $GLOBALS[$language_type] = $languages[array_shift($langcodes)];
    }

    // Proxy request method.
    // We need it to be GET or HEAD. Otherwise, the render cache won't be set.
    /* @see drupal_render_cache_set() */
    $_SERVER['REQUEST_METHOD'] = 'GET';

    return $this;
  }

  /**
   * Builds cache.
   *
   * @return $this
   *
   * @throws DisplayCacheWarmerException
   */
  protected function buildCache() {
    $entity = entity_load_single($this->cacheSpec->entityType, $this->cacheSpec->entityId);
    if (!$entity) {
      throw new DisplayCacheWarmerException('Cannot load entity.', $this->cacheSpec);
    }
    if ($this->cacheSpec->viewMode == 'page_manager') {
      if (!module_exists('page_manager')) {
        throw new DisplayCacheWarmerException('The page_manager module is not enabled.', $this->cacheSpec);
      }
      $entity_type_to_callback_map = array(
        'node' => 'page_manager_node_view_page',
        'taxonomy_term' => 'page_manager_term_view_page',
        'user' => 'page_manager_user_view_page',
      );
      if (!isset($entity_type_to_callback_map[$this->cacheSpec->entityType])) {
        throw new DisplayCacheWarmerException('The entity type is not supported by the page_manager module.', $this->cacheSpec);
      }
      page_manager_get_tasks();
      $entity_type_to_callback_map[$this->cacheSpec->entityType]($entity);
    }
    else {
      $view = entity_view($this->cacheSpec->entityType, array($entity), $this->cacheSpec->viewMode);
      drupal_render($view);
    }
    // Ensure that cache was generated.
    if (!cache_get($this->cacheId, DISPLAY_CACHE_CACHE_BIN)) {
      throw new DisplayCacheWarmerException('The cache was not built for unknown reasons.', $this->cacheSpec);
    }
    return $this;
  }
}
