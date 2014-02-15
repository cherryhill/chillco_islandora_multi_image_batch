<?php

function chillco_islandora_multi_image_batch_form($form, &$form_state, AbstractObject $object) {
  form_load_include($form_state, 'inc', 'chillco_islandora_multi_image_batch', 'includes/batch.form');
  module_load_include('inc', 'islandora', 'includes/utilities');
  $default_namespace = islandora_get_namespace($object->id);

  $form['parent'] = array(
    '#type' => 'value',
    '#value' => $object->id,
  );

  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t('Namespace for created objects?'),
    '#description' => t('Large image objects will be contructed within this namespace.'),
    "#default_value" => $default_namespace,
    '#size' => 15
  );

  $form['email_admin'] = array(
    '#type' => 'checkbox',
    '#title' => t('Notify admin after ingest?'),
    '#description' => t('Whether or not we should notify the site admin that ' .
        'a large image has been ingested after the ingest of the large image completes ' .
        '(requires relevant "Rules" rule).'),
  );

  $form['ingest_set_immediately'] = array(
    '#type' => 'checkbox',
    '#title' => t('Ingest immediately?'),
    '#description' => t('If not selected, objects will just be preprocessed ' .
        'into the queue of objects to be ingested, to be fully-processed (and ' .
        'ingested) later--whenever an admin deems it an appropriate time.'),
    '#default_value' => TRUE,
  );

  $form['chillco_islandora_multi_image_batch_source_xml_directory'] = array(
    '#type' => 'textfield',
    '#title' => 'Source XML directory',
    '#description' => t("Enter the full directory path on where to find the XML for processing."),
    '#default_value' => variable_get('chillco_islandora_multi_image_batch_source_xml_directory', ''),
    '#required' => TRUE,
  );

  $form['chillco_islandora_multi_image_batch_source_image_directory'] = array(
    '#type' => 'textfield',
    '#title' => 'Source Image directory',
    '#description' => t("Enter the full directory path on where to find the matching large images for processing."),
    '#default_value' => variable_get('chillco_islandora_multi_image_batch_source_image_directory', ''),
    '#required' => TRUE,
  );

  return confirm_form($form, 'Import from directories', "islandora/object/{$object->id}/manage/collection");
}


function chillco_islandora_multi_image_batch_form_submit($form, &$form_state) {
  variable_set('chillco_islandora_multi_image_batch_source_image_directory', $form_state['values']['chillco_islandora_multi_image_batch_source_image_directory']);
  variable_set('chillco_islandora_multi_image_batch_source_xml_directory', $form_state['values']['chillco_islandora_multi_image_batch_source_xml_directory']);

  $form_state['values']['target'] = $form_state['values']['chillco_islandora_multi_image_batch_source_xml_directory'];
  $form_state['values']['target_images'] = $form_state['values']['chillco_islandora_multi_image_batch_source_image_directory'];

  $connection = islandora_get_tuque_connection();
  $parameters = $form_state['values'] + array(
    'type' => 'directory',
    'parent_relationship_uri' => 'info:fedora/fedora-system:def/relations-external#',
    'parent_relationship_pred' => 'isMemberOfCollection',
    'wait_for_metadata' => FALSE,
    'directory_dedup' => FALSE,
    'content_models' => array('islandora:chillco_isp_multi_image_cmodel'),
  );

  $preprocessor = new ChillcoIslandoraMultiImageBatch($connection, $parameters);

  if ($form_state['values']['ingest_set_immediately']) {
    islandora_batch_ingest(array(
      'preprocessor' => $preprocessor,
    ));
  }
  else {
    $preprocessed = islandora_batch_handle_preprocessor($preprocessor);
  }
}
