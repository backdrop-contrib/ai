(function ($) {
  Backdrop.behaviors.aiAltFile = {
    attach: function (context, settings) {
      // Check if autogenerate is enabled.
      if (settings.aiAltFile && settings.aiAltFile.autogenerate) {
        $('.ai-alt-file-generate-button', context).once('ai-alt-auto-fire', function () {
          var $button = $(this);
          // Find the wrapper (it might be a sibling or nearby).
          var $wrapper = $('#ai-alt-file-wrapper');
          if (!$wrapper.length) {
            $wrapper = $button.siblings('#ai-alt-file-wrapper');
          }
          var $input = $wrapper.find('input[type="text"], textarea');
          
          // Only trigger if the input is currently empty.
          if ($input.length && $input.val() === '') {
            // Trigger Backdrop AJAX via mousedown.
            $button.mousedown();
          }
        });
      }
    }
  };
})(jQuery);
