<?php

/**
 * Class DisplayCacheWarmerException
 */
class DisplayCacheWarmerException extends Exception {

  /**
   * Constructor.
   *
   * @param string $message
   *   Error message.
   * @param DisplayCacheWarmerCacheSpec|string $cache_spec
   *   Cache specification object, or cache ID.
   */
  public function __construct($message , $cache_spec) {
    if ($cache_spec instanceof DisplayCacheWarmerCacheSpec) {
      $this->message = "$message <br />Cache specification: <pre>" . var_export($cache_spec, TRUE) . '</pre>';
    }
    else {
      $this->message = "$message <br />Cache ID: $cache_spec";
    }
  }
}
