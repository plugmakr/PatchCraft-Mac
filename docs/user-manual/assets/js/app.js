/**
 * PatchCraft User Manual — navigation, scrollspy, mobile sidebar, search filter.
 */
(function () {
  function qs(sel, root) { return (root || document).querySelector(sel); }
  function qsa(sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); }

  function setActive(id) {
    qsa('.sidebar a[href^="#"]').forEach(function (a) {
      a.classList.toggle('active', a.getAttribute('href') === '#' + id);
    });
  }

  function initSmoothScroll() {
    qsa('a[href^="#"]').forEach(function (a) {
      a.addEventListener('click', function (e) {
        var id = a.getAttribute('href').slice(1);
        if (!id) return;
        var target = document.getElementById(id);
        if (!target) return;
        e.preventDefault();
        target.scrollIntoView({ behavior: 'smooth', block: 'start' });
        history.replaceState(null, '', '#' + id);
        setActive(id);
        document.body.classList.remove('nav-open');
      });
    });
  }

  function initScrollSpy() {
    var sections = qsa('main.content section[id]');
    if (!sections.length || !('IntersectionObserver' in window)) return;
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) setActive(entry.target.id);
      });
    }, { rootMargin: '-20% 0px -70% 0px', threshold: 0 });
    sections.forEach(function (s) { observer.observe(s); });
  }

  function initMobileNav() {
    var toggle = qs('.nav-toggle');
    if (!toggle) return;
    toggle.addEventListener('click', function () {
      document.body.classList.toggle('nav-open');
    });
    qs('.nav-backdrop') && qs('.nav-backdrop').addEventListener('click', function () {
      document.body.classList.remove('nav-open');
    });
  }

  function initSearch() {
    var input = qs('#manual-search');
    if (!input) return;
    var items = qsa('.sidebar a[href^="#"]');
    input.addEventListener('input', function () {
      var q = input.value.trim().toLowerCase();
      items.forEach(function (a) {
        var text = (a.textContent || '').toLowerCase();
        var li = a.closest('li');
        if (!li) return;
        li.style.display = !q || text.indexOf(q) !== -1 ? '' : 'none';
      });
    });
  }

  function initHashOnLoad() {
    var hash = (location.hash || '').slice(1);
    if (hash && document.getElementById(hash)) setActive(hash);
  }

  function init() {
    initSmoothScroll();
    initScrollSpy();
    initMobileNav();
    initSearch();
    initHashOnLoad();
  }

  if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', init);
  else
    init();
})();
