<?php

/**
 * @file
 * Implementation of classes from the batch ingest framework for books.
 *
 * Image Container Content Model
 * $multi_image_container_content_model = $connection->repository->constructObject('islandora:chillco_isp_multi_image_container_cmodel');
 *
 * Image Item Content Model.
 * $multi_image_item_content_model = $connection->repository->constructObject('islandora:chillco_isp_multi_image_item_cmodel');
 */

/**
 * Batch preprocessor.
 */
class ChillcoIslandoraMultiImageBatch extends IslandoraScanBatch {

  /**
   * Get the name of the class to instantiate for the batch operations.
   *
   * @overrides IslandoraScanBatch::get_object_class()
   */
  protected static function get_object_class() {
    return 'ChillcoIslandoraMultiImageContainerBatchObject';
  }
  
  /**
   * Allow the pattern to be set differently.
   *
   * @overrides IslandoraScanBatch::get_object_class()
   */
  protected static function get_pattern() {
    return '/.*/';
  }

  /**
   * Generate output analogous to file_scan_directory().
   *
   * @overrides IslandoraScanBatch::scan_zip()
   */
  protected function scan_zip($target) {
    return array();
  }

  /**
   * Get the target resource.
   */
  protected function get_target_images() {
    return $this->parameters['target_images'];
  }

  /**
   * Scan the directory with file_scan_directory().
   *
   * @overrides IslandoraScanBatch::scan_directory()
   */
  protected function scan_directory($target) {
    $files = array();
    $target_images = $this->get_target_images();
    $target_components = explode(DIRECTORY_SEPARATOR, $target);
    $target = implode(DIRECTORY_SEPARATOR, $target_components);
    $target_length = strlen($target) + 1;

    $patterns = array(
      $target,
      '.xml'
    );
    $replacements = array(
      $target_images,
      '-01.jpg',
    );

    $images = file_scan_directory($target_images, '/.*-01.*/');
    foreach (file_scan_directory($target, '/.*.xml$/') as $uri => $value) {
      if (strpos($uri, '.xml') !== FALSE) {
        // Only process xml if there is a matching image to go with it.
        $matching_image = str_replace($patterns, $replacements, $value->uri);
        if (!empty($images[$matching_image])) {
          $files[substr($uri, $target_length)] = $value;
        }
      }
    }
    return $files;
  }

  /**
   * Group file entries logically, to pass off to the import object.
   *
   * @overrides IslandoraScanBatch::group_files()
   */
  protected function group_files($files) {
    $grouped = array();

    foreach ($files as $uri => &$file_object) {
      $file_object->ext = pathinfo($file_object->filename, PATHINFO_EXTENSION);
      $grouped[$file_object->name][$file_object->ext] = $file_object;
      $grouped[$file_object->name]['children'] = $this->get_container_images($file_object);
    }

    return $grouped;
  }

  function get_container_images($file_object) {
    $images = file_scan_directory($this->get_target_images(), "/{$file_object->name}-.*/");
    ksort($images);
    return $images;
  }

  public function preprocess() {
    $added = parent::preprocess();

    $object_type = static::get_object_class();

    foreach ($added as $object) {
      if ($object instanceof $object_type) {
        $children = db_select('islandora_batch_queue', 'q')
          ->fields('q', array('id'))
          ->condition('parent', $object->id);

        // Set pages ready to go.
        db_update('islandora_batch_state')
          ->fields(array(
            'state' => ISLANDORA_BATCH_STATE__READY,
          ))
          ->condition('id', $children, 'IN')
          ->execute();
        // Set book ready to go (after children, that is).
        db_update('islandora_batch_state')
          ->fields(array(
            'state' => ISLANDORA_BATCH_STATE__PENDING_CHILDREN,
          ))
          ->condition('id', $object->id)
          ->execute();
      }
    }

    return $added;
  }
}

/**
 * Flat batch object for processing.
 */
class ChillcoIslandoraFlatBatchObject extends IslandoraScanBatchObject {
  /**
   * Function batch_process.
   *
   * @overrides IslandoraScanBatchObject::batch_process()
   */
  public function batch_process() {
    // Use object_info to create some datastreams.
    $this->label = $this->get_title();
    $this->get_mods();
    $this->get_dc();

    if (!isset($this['IMAGE']) && !empty($this->object_info['jpg'])) {
        $image = $this->object_info['jpg'];
        $image_datastream = $this->constructDatastream('IMAGE');
        // Figure out the mimetype.
        $image_datastream->mimetype = static::get_mime($image);
        $image_datastream->label = check_plain($image->filename);
        $image_datastream->setContentFromFile($image->uri, FALSE);

        $this->ingestDatastream($image_datastream);
    }

    return ISLANDORA_BATCH_STATE__DONE;
  }

  public function get_mods() {
    if (!isset($this['MODS'])) {
      $mods_datastream = $this->constructDatastream('MODS', 'X');
      $mods_datastream->mimetype = 'text/xml';
      $mods_datastream->label = 'MODS Record';

      if (isset($this->object_info['xml'])) {
        $xml = file_get_contents($this->object_info['xml']->uri);
        $mods_datastream->content = chillco_dpldams_xmlparser_transform($xml);
      }
      else {
        $title = $this->base_name;
        $mods_datastream->content = <<<EOXML
<mods xmlns:mods="http://www.loc.gov/mods/v3" xmlns="http://www.loc.gov/mods/v3">
  <titleInfo>
    <title>$title</title>
  </titleInfo>
  <identifier type="resourceID">$title</identifier>
</mods>
EOXML;
      }
      $this->ingestDatastream($mods_datastream);
    }
    return $this['MODS']->content;
  }
}

class ChillcoIslandoraMultiImageContainerBatchObject extends ChillcoIslandoraFlatBatchObject {

  /**
   * Constructor for the ChillcoIslandoraMultiImageContainerBatchObject.
   */
  public function __construct(IslandoraTuque $connection, $base_name, $object_info, $preprocessor_parameters) {
    IslandoraBatchObject::__construct(NULL, $connection->repository);

    $this->base_name = $base_name;
    $this->object_info = $object_info;
    $this->resources = array();
    $this->preprocessor_parameters = $preprocessor_parameters;
  }

  public function initial_batch_state() {
    // Not ready until we've added the image items.
    return ISLANDORA_BATCH_STATE__NOT_READY;
  }
  
  public function batch_process() {
    parent::batch_process();
    module_load_include('inc', 'chillco_islandora_solution_pack_multi_image');
    chillco_islandora_solution_pack_large_image_update_container_asset($this);
  }

  public function get_children(IslandoraTuque $connection) {
    // Create page objects, return in an array.
    $children = array();

    $sequence = 0;
    foreach ($this->object_info['children'] as $filepath => $info) {
      $sequence++;
      $object = array();
      $file_info = pathinfo($filepath);
      $object[$file_info['extension']] = $info;
      $children[] = new ChillcoIslandoraMultiImageItemBatchObject($connection, $this->id, $sequence, $object, $this->preprocessor_parameters);
    }

    dpm($children);
    return $children;
  }
}

class ChillcoIslandoraMultiImageItemBatchObject extends ChillcoIslandoraFlatBatchObject {
  protected $parent_id, $sequence_number;

  public function __construct(IslandoraTuque $connection, $parent_id, $sequence, $object_info, $pp) {
    parent::__construct($connection, "$parent_id-$sequence", $object_info, $pp);

    $this->parent_id = $parent_id;
    $this->sequence_number = $sequence;
  }

  public function get_children(IslandoraTuque $connection) {
    return array();
  }

  public function initial_batch_state() {
    return ISLANDORA_BATCH_STATE__NOT_READY;
  }

  public function add_relationships() {
    module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
    module_load_include('inc', 'chillco_islandora_solution_pack_multi_image');

    // Add relationship to collection.
    $rels_ext = $this->relationships;
    islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageOf', $this->parent_id);
    islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', (string) $this->sequence_number, TRUE);
    islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageNumber', (string) $this->sequence_number, TRUE);
    islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSection', '1', TRUE);
    islandora_paged_content_set_relationship($rels_ext, FEDORA_RELS_EXT_URI, 'isMemberOf', $this->parent_id);
    // Add content model relationship.
    $this->models = 'islandora:chillco_isp_multi_image_item_cmodel';
  }
}
