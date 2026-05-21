(function (window, document, $, Drupal) {
  'use strict';

  function getPreviewRoot() {
    var el = document.getElementById('me-preview') || document.getElementById('me_preview');
    if (!el) {
      return null;
    }
    if (el.tagName && el.tagName.toLowerCase() === 'iframe') {
      try {
        return el.contentDocument || (el.contentWindow && el.contentWindow.document) || null;
      }
      catch (e) {
        return null;
      }
    }
    return el;
  }

  function extractPreviewText() {
    var root = getPreviewRoot() || document;
    var body = root.body || root;
    var text = (body && body.textContent) ? body.textContent : '';
    text = String(text || '').replace(/\s+/g, ' ').trim();
    if (text.length > 12000) {
      text = text.substring(0, 12000);
    }
    return text;
  }

  function setHiddenPreviewText(value) {
    var input = document.querySelector('input[name="ai_content_preview_text"]');
    if (!input) {
      return;
    }
    input.value = value || '';
    if (typeof $ === 'function') {
      $(input).trigger('change');
    }
  }

  function bindButtons(context) {
    var $ctx = typeof $ === 'function' ? $(context) : null;
    var $buttons = $ctx ? $ctx.find('.ai-content-action-button') : $('.ai-content-action-button');

    $buttons.each(function () {
      var el = this;
      if (el._aiPreviewBound) {
        return;
      }
      el._aiPreviewBound = true;
      el.addEventListener('click', function () {
        setHiddenPreviewText(extractPreviewText());
      });
    });
  }

  Drupal = Drupal || window.Drupal || {};
  Drupal.behaviors = Drupal.behaviors || {};
  Drupal.behaviors.aiContentPreviewExtract = {
    attach: function (context) {
      bindButtons(context || document);
    }
  };
})(window, document, (window.jQuery || window.$), window.Drupal);
