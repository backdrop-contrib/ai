(function ($) {
  function triggerAutoGenerate(fid, wrapperId, token) {
    if (!fid || !token) {
      return;
    }

    $.ajax({
      url: Backdrop.settings.basePath + 'ai-alt/ckeditor-autogenerate',
      type: 'POST',
      data: {
        fid: fid,
        token: token
      },
      success: function (response) {
        if (response.status === 'success' && response.alt_text) {
          $('#' + wrapperId + ' input[name="attributes[alt]"]').val(response.alt_text);
        }
      }
    });
  }

  Backdrop.behaviors.aiAltCkeditor = {
    attach: function (context, settings) {
      var aiAlt = settings.aiAlt || {};
      var wrapperId = aiAlt.wrapper_id;
      var token = aiAlt.token;
      if (!wrapperId || !token) {
        return;
      }

      $(document).once('ai-alt-ckeditor-handlers').each(function () {
        var existingAlt = $('input[name="attributes[alt]"]').val();
        var existingFid = $('input[name="fid[fid]"]').val();
        if (!existingAlt && existingFid && existingFid !== '0') {
          triggerAutoGenerate(existingFid, wrapperId, token);
        }

        $(document).on('change', 'input[name="files[fid]"]', function () {
          var attempts = 0;
          var checkForFid = setInterval(function () {
            var fid = $('input[name="fid[fid]"]').val();
            attempts++;
            if (fid && fid !== '0') {
              clearInterval(checkForFid);
              triggerAutoGenerate(fid, wrapperId, token);
            }
            else if (attempts >= 20) {
              clearInterval(checkForFid);
            }
          }, 500);
        });

        $(document).on('click', '.image-library-choose-file', function () {
          var selectedImage = $(this).find('img');
          var fid = selectedImage.data('fid');
          triggerAutoGenerate(fid, wrapperId, token);
        });

        $(document).on('click', '.editor-dialog .form-actions input[type="submit"]', function () {
          var altText = $('input[name="attributes[alt]"]').val();
          var fid = $('input[name="fid[fid]"]').val();
          var imgSrc = $('input[name="attributes[src]"]').val();
          if (!imgSrc) {
            return;
          }

          $('img').filter(function () {
            return $(this).attr('src') === imgSrc;
          }).first().attr('alt', altText).attr('data-file-id', fid || '');
        });
      });
    }
  };
})(jQuery);
