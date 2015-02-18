<?php

/**
 * Cache specification.
 */
class DisplayCacheWarmerCacheSpec {

  public $cacheId;

  public $entityType;
  public $entityId;
  public $viewMode;
  public $element;

  public $contentLanguage;

  public $domainId;
  public $domainVariantId;

  public $themeName;

  /**
   * The indexed array of language codes. The order should coincide with the
   * order of the language_types_configurable() return.
   *
   * @var array
   */
  public $langcodes = array();

  /**
   * The associative array of the user role IDs:
   *   - values are numeric *strings*
   *   - keys repeat values
   *   - sorted with asort()
   *
   * @var array
   */
  public $userRoleIds = array();

  public $userId;
  public $url;
}
