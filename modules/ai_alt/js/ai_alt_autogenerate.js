(function ($) {
  $(document).ready(function () {

    /**
     * 1) Wrap ALT fields in a container if not already done.
     */
    function wrapAltTextFields() {
      $("input[name$='[alt]']").each(function () {
        var $altField = $(this).closest(".form-item");
        if (!$altField.parent().hasClass("ai-alt-field-wrapper")) {
          $altField.wrap('<div class="ai-alt-field-wrapper"></div>');
        }
      });
    }

    /**
     * 2) For each entry in Backdrop.settings.aiAlt, generate alt text if needed.
     *    We only add items in the PHP if alt == '', so each item is auto-generated once.
     */
    function triggerAutoGenerationForAll() {
      if (Backdrop.settings.aiAlt) {
        $.each(Backdrop.settings.aiAlt, function (key, item) {
          // item is { fid, field_name, delta, target_id } for each image
          if (item.fid && item.field_name !== undefined && item.delta !== undefined) {
            generateAltText(item.fid, item.field_name, item.delta, key);
          }
        });
      }
    }

    /**
     * 3) Helper: Fire an AJAX request to get alt text, then store it in the correct <input>.
     *    After success, remove that item from aiAlt so it’s not re-run.
     */
    function generateAltText(fid, fieldName, delta, key) {
      $.ajax({
        url: Backdrop.settings.basePath + "ai-alt/generate-alt-text",
        type: "POST",
        data: { fid: fid, field_name: fieldName, delta: delta },
        success: function (response) {
          if (response.status === "success" && response.alt_text) {
            var selector = "input[name='" + fieldName + "[und][" + delta + "][alt]']";
            $(selector).val(response.alt_text).trigger('change');

            // Remove from aiAlt so we do not re-run.
            if (Backdrop.settings.aiAlt[key]) {
              delete Backdrop.settings.aiAlt[key];
            }
          }
        },
        error: function (xhr, status, error) {
          console.error("AJAX request failed:", status, error);
        }
      });
    }

    /**
     * 4) On AJAX complete, if a new image was added, the form might reattach auto-generate.
     *    So we re-run triggerAutoGenerationForAll to catch newly empty alt fields only.
     */
    $(document).ajaxComplete(function (event, xhr, settings) {
      // If a file was uploaded
      if (settings.url.includes("file/ajax")) {
        setTimeout(triggerAutoGenerationForAll, 500);
      }
    });

    /**
     * 5) Optionally, handle remove button if you want to clear aiAlt for that item.
     *    You can do so if your remove button includes data-field-name, data-delta, etc.
     */
    $(document).on("click", ".file-remove-button", function () {
      // If you have data attributes, parse them, then remove from aiAlt if you wish.
      // For now, do nothing or the simplest approach might be:
      //   Backdrop.settings.aiAlt = {};
      // But that would remove all items, so be careful.
    });

    // **Run initial setup**
    wrapAltTextFields();
    triggerAutoGenerationForAll();
  });
})(jQuery);
