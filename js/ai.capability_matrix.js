/**
 * @file
 * All / None selection and enable/disable for the model capability matrix.
 *
 * A bulk control carrying data-ai-capability is column-scoped (one capability,
 * every model); one without it is row-scoped (one model, every capability) and
 * operates within its own row.
 *
 * Either way only columns under manual override are touched, because the rest
 * are showing what the provider reports and their checkboxes are disabled.
 *
 * Model checkboxes are enabled/disabled here rather than via core's #states:
 * a large catalog (OpenRouter alone is ~600 models) produces thousands of
 * matrix cells, and #states registers one JS watcher per element -- at that
 * scale it locks up the browser on page load. This does the same job with a
 * single delegated handler per table instead.
 */
(function ($) {

  'use strict';

  /**
   * Map of capability => whether its column is currently overridden.
   */
  function capabilityStates($table) {
    var states = {};
    $table.find('input.ai-capability-override-toggle').each(function () {
      states[this.getAttribute('data-ai-capability')] = this.checked;
    });
    return states;
  }

  /**
   * The model checkboxes a bulk control acts on, before the editable filter.
   */
  function targetBoxes($table, $bulk) {
    var capability = $bulk.attr('data-ai-capability');
    var $scope = capability
      ? $table.find('input[type="checkbox"][data-ai-capability="' + capability + '"]')
      : $bulk.closest('tr').find('input[type="checkbox"][data-ai-capability]');

    return $scope.not('.ai-capability-override-toggle');
  }

  /**
   * Enable/disable the model checkboxes and bulk controls to match which
   * columns are currently overridden.
   */
  function refresh($table) {
    var states = capabilityStates($table);
    var anyEditable = false;
    var capability;

    for (capability in states) {
      if (states[capability]) {
        anyEditable = true;
        break;
      }
    }

    for (capability in states) {
      $table.find('input[type="checkbox"][data-ai-capability="' + capability + '"]')
        .not('.ai-capability-override-toggle')
        .prop('disabled', !states[capability]);
    }

    $table.find('.ai-capability-bulk').each(function () {
      var $bulk = $(this);
      var own = $bulk.attr('data-ai-capability');
      // A row control is useful as soon as any one column is overridden.
      var editable = own ? !!states[own] : anyEditable;

      $bulk.toggleClass('is-disabled', !editable);
      $bulk.find('.ai-capability-bulk__button').prop('disabled', !editable);
    });
  }

  Backdrop.behaviors.aiCapabilityMatrix = {
    attach: function (context) {
      $('.ai-capability-matrix', context).once('ai-capability-bulk', function () {
        var $table = $(this);

        $table.on('click', '.ai-capability-bulk__button', function (event) {
          event.preventDefault();

          var $bulk = $(this).closest('.ai-capability-bulk');
          var checked = $(this).attr('data-ai-bulk') === 'all';
          var states = capabilityStates($table);

          targetBoxes($table, $bulk).each(function () {
            if (!states[this.getAttribute('data-ai-capability')]) {
              // Column is on auto; leave it to the provider.
              return;
            }
            if (this.checked !== checked) {
              this.checked = checked;
              // Let any other change handlers see the update.
              $(this).trigger('change');
            }
          });
        });

        $table.on('change', '.ai-capability-override-toggle', function () {
          refresh($table);
        });

        refresh($table);
      });
    }
  };

})(jQuery);
