(function ($) {
  var requests = {};

  function findAltInput(fieldName, fieldLang, delta) {
    var name = fieldName + '[' + fieldLang + '][' + delta + '][alt]';
    return $('input[name]').filter(function () {
      return this.name === name;
    }).first();
  }

  function removePendingItem(settings, key) {
    if (settings.aiAlt && settings.aiAlt[key]) {
      delete settings.aiAlt[key];
    }
  }

  function generateAltText(settings, fid, fieldName, fieldLang, delta, key) {
    var requestKey = fieldName + ':' + fieldLang + ':' + delta + ':' + fid;
    if (requests[requestKey]) {
      return;
    }
    requests[requestKey] = true;

    $.ajax({
      url: settings.basePath + 'ai-alt/generate-alt-text',
      type: 'POST',
      data: {
        fid: fid,
        token: settings.aiAlt[key].token
      },
      success: function (response) {
        if (response.status === 'success' && response.alt_text) {
          findAltInput(fieldName, fieldLang, delta).val(response.alt_text).trigger('change');
        }
      },
      complete: function () {
        delete requests[requestKey];
        removePendingItem(settings, key);
      }
    });
  }

  function triggerAutoGeneration(settings) {
    if (!settings.aiAlt) {
      return;
    }

    $.each(settings.aiAlt, function (key, item) {
      var fieldLang = item.field_lang || 'und';
      if (item.fid && item.field_name !== undefined && item.delta !== undefined && item.token) {
        generateAltText(settings, item.fid, item.field_name, fieldLang, item.delta, key);
      }
    });
  }

  Backdrop.behaviors.aiAltAutogenerate = {
    attach: function (context, settings) {
      $('input[name$="[alt]"]', context).once('ai-alt-field-wrapper').each(function () {
        var $altField = $(this).closest('.form-item');
        if (!$altField.parent().hasClass('ai-alt-field-wrapper')) {
          $altField.wrap('<div class="ai-alt-field-wrapper"></div>');
        }
      });
      triggerAutoGeneration(settings);
    }
  };
})(jQuery);
