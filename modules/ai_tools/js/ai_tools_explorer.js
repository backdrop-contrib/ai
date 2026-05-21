(function ($, Backdrop) {
  'use strict';

  Backdrop.behaviors.aiToolsExplorerSelectionControls = {
    attach: function (context) {
      var $context = $(context);
      var $root = $context.find('.ai-tools-explorer-cards').once('ai-tools-explorer-controls');

      $root.each(function () {
        var $cards = $(this);
        var $fieldset = $cards.closest('fieldset');
        var $selectAll = $fieldset.find('.ai-tools-select-all').first();
        var $clearAll = $fieldset.find('.ai-tools-clear-all').first();

        if ($selectAll.length) {
          $selectAll.on('click', function (e) {
            e.preventDefault();
            $cards.find('input[type="checkbox"]').prop('checked', true);
          });
        }

        if ($clearAll.length) {
          $clearAll.on('click', function (e) {
            e.preventDefault();
            $cards.find('input[type="checkbox"]').prop('checked', false);
          });
        }
      });
    }
  };
})(jQuery, Backdrop);

