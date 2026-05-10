// Tutorial site nav helpers — highlights the current section in the
// sidebar based on the page filename and adds smooth-scroll anchors.
(function () {
    function init() {
        var here = (location.pathname.split('/').pop() || 'index.html').toLowerCase();
        var links = document.querySelectorAll('.sidebar a[href]');
        for (var i = 0; i < links.length; i++) {
            var href = (links[i].getAttribute('href') || '').toLowerCase();
            if (href === here || (here === '' && href.indexOf('index') === 0)) {
                links[i].classList.add('active');
            }
        }

        // Smooth-scroll for in-page anchors.
        document.querySelectorAll('a[href^="#"]').forEach(function (a) {
            a.addEventListener('click', function (e) {
                var id = a.getAttribute('href').slice(1);
                if (!id) return;
                var target = document.getElementById(id);
                if (target) {
                    e.preventDefault();
                    target.scrollIntoView({ behavior: 'smooth', block: 'start' });
                    history.replaceState(null, '', '#' + id);
                }
            });
        });
    }

    if (document.readyState === 'loading')
        document.addEventListener('DOMContentLoaded', init);
    else
        init();
})();
