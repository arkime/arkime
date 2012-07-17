(function($, undefined){

    $(function() {
        if (!window.prettyPrint) {
            return;
        }
        
        $('.showcase').each(function(){
            
            var $this = $(that || this),
                text, nodeName, lang, that;
            
            if ($this.data('showcaseImport')) {
                $this = $($this.data('showcaseImport'));
                that = $this.get(0);
            }
            
            nodeName = (that || this).nodeName.toLowerCase();
            lang = nodeName == 'script' 
                ? 'js' 
                : (nodeName == 'style' ? 'css' : 'html');
            
            if (lang == 'html') {
                text = $('<div></div>').append($this.clone()).html();
            } else {
                text = $this.text();
            }

            $('<pre class="prettyprint lang-'+ lang +'"></pre>')
                .text(text)
                .insertBefore(this);
            
            that && $(this).remove();
        });

        prettyPrint();
    });

})(jQuery);