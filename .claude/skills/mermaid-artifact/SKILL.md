---
name: mermaid-artifact
description: "Publish a Mermaid diagram (flowchart, sequence, class, state, ER, timeline…) as an Artifact web page, or repair one that renders blank. Use when a diagram should become a shareable page, when adapting an existing .html mermaid draft for publishing, or when a published artifact shows raw mermaid source, an empty box, or run-together node labels instead of a diagram. Covers the artifact CSP (never import the library), the <pre class=\"mermaid\"> contract, the escaping that keeps line breaks alive, and how to style the wrapper the runtime injects."
---

# Mermaid diagrams as Artifacts

Artifacts render Mermaid **natively**. The whole job is handing the runtime a correctly
escaped source block and styling what it builds — every failure below is silent, so they
are worth knowing before you write the page rather than after.

## Never load the library

No `import`, no `<script src>`, no inlined copy. The artifact CSP admits scripts only from
cdnjs, `cdn.jsdelivr.net/npm/`, `cdn.tailwindcss.com` and `code.jquery.com` — **esm.sh and
unpkg are blocked by name**, and a blocked script fails with no console error and no visible
sign beyond a page that never draws. A draft written for a normal web page almost always
opens with `import mermaid from 'https://esm.sh/mermaid@11/…'`: delete the import and the
entire `<script>` around it. Any `mermaid.initialize({…})` config in that draft is dead
code too — the runtime sets its own (see below) and never reads yours.

## The block

HTML pages: put the source in `<pre class="mermaid">`. Markdown pages: a ```mermaid fence.

```html
<pre class="mermaid">flowchart LR
  A["Agent&lt;br&gt;AgentSpec · AgentResult"] --&gt; B["StatusBoard"]
</pre>
```

## Escape `<` in the source — this is the silent one

Everything inside the `pre` is parsed as HTML before Mermaid ever sees it, and Mermaid reads
`textContent`. A literal `<br>` therefore becomes a *DOM element*, and `textContent` drops
it — so `"Phase Review<br>3 reviewer Agents"` reaches Mermaid as `"Phase Review3 reviewer
Agents"`. The diagram still renders, which is why this one gets shipped: labels are simply
run together.

Write `&lt;br&gt;`, and escape `<` anywhere else it appears in a label. `-->` needs no
escaping (`>` alone is fine in text), though `--&gt;` is harmless if you escape uniformly.

Verify the round-trip before publishing — unescape the block and diff it against the source
you were given:

```python
import re, html
pre = re.search(r'(?s)<pre class="mermaid">(.*?)</pre>', open(path).read()).group(1)
txt = html.unescape(pre)          # what the browser hands Mermaid
for token in ["A --> B", "Phase Review<br>3 reviewer Agents"]:
    assert token in txt, token
```

## What the runtime does to your page

It appends itself at publish time, between `<!--claude-mermaid-runtime-begin-->` markers,
and on load it:

1. claims every `pre.mermaid`, marking it `data-claude-mermaid-claimed`;
2. inserts a **`div.mermaid-diagram` before the `pre`** and sets the `pre` to
   `display:none` — on a render error it removes the div and restores the `pre` instead;
3. injects `.mermaid-diagram svg { max-width:100%; height:auto }` and renders at natural
   size (`useMaxWidth:false`), so the SVG is *scaled down* to fit its container;
4. initializes Mermaid itself with `theme:'base'`, `securityLevel:'strict'` and its own
   light/dark palette, re-rendering when `prefers-color-scheme` changes or when
   `data-theme` changes on the root element.

Three consequences worth designing around:

- **Style `.mermaid-diagram`, not `pre.mermaid`.** The `pre` is hidden; rules aimed at it
  (or at `.mermaid svg`) never apply to the rendered diagram. Do style the `pre` as the
  fallback — mono, small — since it reappears if rendering fails.
- **`body`'s font becomes the diagram's font.** The runtime reads
  `getComputedStyle(document.body).fontFamily`. A serif body face puts `AgentSpec` in a
  serif; set `body` to the face the diagram should use and scope other faces to their
  own elements.
- **`body`'s background becomes the diagram's background.** It walks `body` then
  `documentElement` for the first non-transparent computed background. Paint `body` from a
  theme token, or the diagram grounds itself on the runtime's default instead of your page.

## Wide diagrams

`max-width:100%` means a wide `flowchart LR` shrinks rather than scrolls, and at phone width
it shrinks into illegibility. Put a floor under it inside a scrolling container — costs
nothing on desktop, where the container is already wider:

```css
.figwrap { overflow-x: auto; }
.mermaid-diagram { min-width: 760px; }
```

## Verify after publishing

Rendering can only be confirmed live. Publish, then `action: "read"` the artifact and check
the `claude-mermaid-runtime` block is present and that your `pre` carries the source you
expect. Reading it is also how you learn what the current runtime does — the details above
came from reading it, and it is versioned (`mermaid-11.16.1.min.js` at the time of writing).

## Post-render DOM edits

Drafts often tweak the output after rendering (`querySelectorAll('.node rect')` to round
corners, recolor, retitle). The runtime owns render timing, so that code has nothing to hook
into — it needs a `MutationObserver` on the `.mermaid-diagram`, re-applied on every theme
re-render. Usually not worth it: prefer Mermaid's own `classDef`/`style` statements in the
source, which survive re-renders for free.

## The page around it

Normal artifact rules still apply — a real `<title>`, theme tokens defined on bare `:root`
and redefined for both dark states, a side gutter at every width. `template.html` beside
this file is a working starting point: swap in your diagram source and the header copy.
