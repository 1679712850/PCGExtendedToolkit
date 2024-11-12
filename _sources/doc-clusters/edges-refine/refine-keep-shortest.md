---
layout: page
grand_parent: Clusters
parent: Refine
title: 🝔 Keep Shortest
subtitle: Keeps the shortest edge of each node
#summary: The **Keep Shortest** refinement ...
color: blue
splash: icons/icon_edges-refine.svg
see_also:
    - Refine
    - 🝔 Keep Longest
tagged: 
    - edgerefining
nav_order: 21
---

{% include header_card_toc %}

This refinement **keeps a single connected edge for each point**: the one with the **shortest** length.
{: .fs-5 .fw-400 } 

>Note that the remaining `Edge` can be the same for multiple, different `Vtx`.

{% include img a='details/edges-refine/refine-keep-shortest.png' %}
