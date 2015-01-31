# Display Cache Warmer

A drupal module that warms caches of the [Display Cache](https://www.drupal.org/project/display_cache) module.

On a cron run, the module checks the  Display Cache caches that have been actively used and recreates missing ones.

## Before installation

The module requires [AmazeeLabs fork](https://github.com/AmazeeLabs/heisencache) of the [Heisencache module](https://github.com/FGM/heisencache).

Install it with:

    git submodule add git@github.com:AmazeeLabs/heisencache.git sites/all/modules/custom/heisencache
    drush en heisencache -y

Next you will need to configure the `CacheReadLogWriterSubscriber` to start monitor the Display Cache caches.

    <?php
    
    // Contents of the sites/default/settings.heisencache.inc file.
    
    namespace OSInet\Heisencache;
    
    Config::instance()->getEmitter()
      ->register(new CacheReadLogWriterSubscriber(array(
        'cache_display_cache',
      )))
    ;

This file is not used until you set the Heisencache as the last cache backend. 

    // Part of the sites/default/settings.local.php file.
    
    // If you have other cache backends set, they should go before the
    // Heisencache.
    $conf['cache_backends'][] = 'sites/all/modules/redis/redis.autoload.inc';
    
    // Set Heisencache as the last cache backend.
    $conf['cache_backends'][] = 'sites/all/modules/heisencache/heisencache.inc';
    
    // If you use the Domain Access module, or any other modules that bootstraps
    // the Drupal in the setting.php, the includes of such modules should go
    // after the Heisencache configuration.
    include DRUPAL_ROOT . '/sites/all/modules/domain/settings.inc';

Once you have configured the Heisencache, it will start monitor the `cache_get()` statistics for the Display Cache caches.

## Installation and configuration
 
Install as usual:

    git submodule add git@github.com:AmazeeLabs/display_cache_warmer.git sites/all/modules/custom/display_cache_warmer
    drush en display_cache_warmer -y

Module settings could be found at the `admin/config/development/display-cache-warmer` path.

WARNING: once `display_cache_warmer_cron()` is called it will not stop until all caches are rebuilt.

To not break other cron tasks execution:

1. Use [Elysia Cron](https://www.drupal.org/project/elysia_cron) or similar module.
1. Settings for Elysia Cron:
    1. Use minimal difference between "Time limit" and "Stuck time" settings.
    1. Place `display_cache_warmer_cron` in a separate channel.
1. Run every minute (`* * * * *`).
