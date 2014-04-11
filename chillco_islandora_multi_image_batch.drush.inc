<?php

/**
 * Implements hook_drush_command().
 */
function chillco_islandora_multi_image_batch_drush_command() {
  $items = array();

  $items['chillco_islandora_multi_image_batch_preprocess'] = array(
    'aliases' => array('cimibp'),
    'description' => 'Preprocessed multi image collections into database entries.',
    'options' => array(
      'target_img' => array(
        'description' => 'The target directory to scan for images.',
        'required' => TRUE,
      ),
      'target_xml' => array(
        'description' => 'The target directory to scan for xml files.',
        'required' => TRUE,
      ),
      'parent' => array(
        'description' => 'Parent collection pid.',
        'required' => TRUE,
      ),
      'namespace' => array(
        'description' => 'The namespace for objects created by this command.  Defaults to namespce set in fedora config.',
        'required' => FALSE,
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );

  return $items;
}

/**
 * Batch preprocess the images.
 */
function drush_chillco_islandora_multi_image_batch_preprocess() {
  drupal_static_reset('islandora_get_tuque_connection');
  $connection = islandora_get_tuque_connection();
  $parameters = array(
    'type' => 'directory',
    'namespace' => drush_get_option('namespace', 'islandora'),
    'parent' => drush_get_option('parent'),
    'target' => drush_get_option('target_xml'),
    'target_images' => drush_get_option('target_img'),
    'parent_relationship_uri' => 'info:fedora/fedora-system:def/relations-external#',
    'parent_relationship_pred' => 'isMemberOfCollection',
    'wait_for_metadata' => FALSE,
    'directory_dedup' => FALSE,
    'content_models' => array('islandora:chillco_isp_multi_image_container_cmodel'),
  );

  $preprocessor = new ChillcoIslandoraMultiImageBatch($connection, $parameters);

  // Pass the preprocessor off to run.
  $preprocessed = islandora_batch_handle_preprocessor($preprocessor);
}
