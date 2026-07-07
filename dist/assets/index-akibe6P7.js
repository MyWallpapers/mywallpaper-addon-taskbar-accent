(function(){const a=document.createElement("link").relList;if(a&&a.supports&&a.supports("modulepreload"))return;for(const e of document.querySelectorAll('link[rel="modulepreload"]'))l(e);new MutationObserver(e=>{for(const t of e)if(t.type==="childList")for(const i of t.addedNodes)i.tagName==="LINK"&&i.rel==="modulepreload"&&l(i)}).observe(document,{childList:!0,subtree:!0});function p(e){const t={};return e.integrity&&(t.integrity=e.integrity),e.referrerPolicy&&(t.referrerPolicy=e.referrerPolicy),e.crossOrigin==="use-credentials"?t.credentials="include":e.crossOrigin==="anonymous"?t.credentials="omit":t.credentials="same-origin",t}function l(e){if(e.ep)return;e.ep=!0;const t=p(e);fetch(e.href,t)}})();const f={enabled:!0,mode:"blur",accentColor:"#1f8fff",opacity:128,showStatus:!0},s=window.MyWallpaper?.layer,o=s?.root??document.body;o.classList.add("taskbar-accent-root");o.style.width="100%";o.style.height="100%";o.style.margin="0";o.style.overflow="hidden";o.style.pointerEvents="none";s?.setPointerEvents?.("none");const c=document.createElement("style");c.textContent=`
  html, body {
    width: 100%;
    height: 100%;
    margin: 0;
    overflow: hidden;
    background: transparent;
  }

  .taskbar-accent-status {
    box-sizing: border-box;
    width: 100%;
    height: 100%;
    display: grid;
    place-items: center;
    padding: 16px;
    color: white;
    font: 600 18px/1.3 Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    text-align: center;
    text-shadow: 0 2px 18px rgba(0, 0, 0, 0.55);
    background: radial-gradient(circle at 50% 50%, rgba(31, 143, 255, 0.30), transparent 62%);
  }

  .taskbar-accent-status small {
    display: block;
    margin-top: 6px;
    font-size: 12px;
    font-weight: 500;
    opacity: 0.75;
  }
`;document.head.append(c);const n=document.createElement("div");n.className="taskbar-accent-status";o.append(n);function d(r){return{...f,...r}}function u(r){n.hidden=!r.showStatus,n.innerHTML=`
    <div>
      Taskbar Accent
      <small>${r.enabled?`${r.mode} ${r.accentColor} / ${r.opacity}`:"disabled"}</small>
    </div>
  `}u(d(s?.settings.get()??{}));const m=s?.settings.subscribe(r=>u(d(r)));s?.lifecycle?.onDispose(()=>{m?.(),c.remove(),n.remove()});
