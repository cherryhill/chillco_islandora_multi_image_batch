<?php

/**
 * @file
 * Implementation of classes from the batch ingest framework for books.
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
    return 'ChillcoIslandoraLargeImageBatchObject';
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
      '.jpg',
    );

    $images = file_scan_directory($target_images, $this->get_pattern());
    foreach (file_scan_directory($target, $this->get_pattern()) as $uri => $value) {
      if (strpos($uri, '.xml') !== FALSE) {
        // Only process xml if there is a matching image to go with it.
        $matching_image = str_replace($patterns, $replacements, $value->uri);
        if (!empty($images[$matching_image])) {
          $files[substr($uri, $target_length)] = $value;
          $files[substr($matching_image, $target_length)] = $images[$matching_image];
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

    // TODO: Group according to basename, to and hand-off each group to object
    // wrappers.
    foreach ($files as $uri => &$file_object) {
      $file_object->ext = pathinfo($file_object->filename, PATHINFO_EXTENSION);
      $grouped[$file_object->name][$file_object->ext] = $file_object;
    }

    return $grouped;
  }
}

class ChillcoIslandoraLargeImageBatchObject extends IslandoraScanBatchObject {

  /**
   * Constructor for the IslandoraScanBatchObject.
   */
  public function __construct(IslandoraTuque $connection, $base_name, $object_info, $preprocessor_parameters) {
    IslandoraBatchObject::__construct(NULL, $connection->repository);

    $this->base_name = $base_name;
    $this->object_info = $object_info;
    $this->resources = array();
    $this->preprocessor_parameters = $preprocessor_parameters;
  }

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
</mods>
EOXML;
      }
      $this->ingestDatastream($mods_datastream);
    }
    return $this['MODS']->content;
  }
}
