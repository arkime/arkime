(function() {

  'use strict';

  /**
   * @class UploadController
   * @classdesc Interacts with moloch upload page
   * @example
   * '<moloch-upload></moloch-upload>'
   */
  class UploadController {

    /**
     * Initialize global variables for this controller
     * @param FileUploader angular-file-upload module
     *                     github.com/nervgh/angular-file-upload
     *
     * @ngInject
     */
    constructor(FileUploader) {
      this.FileUploader = FileUploader;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.uploader = new this.FileUploader({ url: 'upload' });
      
      this.uploader.onCompleteItem = () => {
        this.uploading = false;
        this.remove();
      };
    }

    upload() {
      this.uploading = true;

      let file = this.uploader.queue[0];

      if (file) {
        file.formData.push({ tag: this.tag }); // add tag to upload
        file.upload();
      }
    }

    cancel() {
      if (this.uploader.queue[0]) {
        this.uploader.queue[0].cancel();
      }
    }

    remove() {
      if (this.uploader.queue[0]) {
        this.uploader.queue[0].remove();
        angular.element('input[type="file"]').val(null);
      }
    }

  }

  UploadController.$inject = ['FileUploader'];

  /**
   * Moloch Upload Directive
   * Displays upload form
   */
  angular.module('moloch')
    .component('molochUpload', {
      template  : require('html!./upload.html'),
      controller: UploadController
    });

})();
