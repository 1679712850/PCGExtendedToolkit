---
layout: page
family: Pathfinding
#grand_parent: All Nodes
parent: Pathfinding
title: Edges Pathfinding
subtitle: Find paths connecting seeds to goals.
summary: The **Edges Pathfinding** mode ...
color: white
splash: icons/icon_pathfinding-edges.svg
tagged: 
    - node
    - pathfinder
see_also: 
    - Pathfinding
nav_order: 1
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Seeds
        desc : Seed points
        pin : points
    -   name : Goals
        desc : Goal points
        pin : points
    -   name : Heuristics
        desc : 🝰 Heuristics nodes that will be used by the pathfinding search algorithm
        pin : params
outputs:
    -   name : Paths
        desc : A point collection per path found
        pin : points
---

{% include header_card_node %}

The **Edges Pathfinding** node ...
{: .fs-5 .fw-400 } 

{% include img a='details/details-pathfinding-edges-plot.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Goal Picker**| The module that will be used to interpret and manipulate goals and seeds inputs.<br>*Each module has individual settings and documentation -- See [Available Goal Pickers](#available--goal-pickers-modules).*|

| Property       | Description          |
|:-------------|:------------------|
|**Plot inclusiong**||
| Add Seed to Path           | Prepends the *seed position* at the beginning of the output path.<br>*This will create a point with the position of the seed.* |
| Add Goal to Path           | Appends the *goal position* at the end of the output path.<br>*This will create a point with the position of the goal.* |
| Path Composition           | Whether the output paths are made from `Vtx` or `Edges` points. |

|**Picking**||
| Seed Picking         | Lets you control how the seed node (`Vtx`) will be picked based on the provided seed position. |
| Goal Picking         | Lets you control how the goal node (`Vtx`) will be picked based on the provided goal position. |
| **Search Algorithm**         | Let you pick which {% include lk id='⊚ Search' %} algorithm to use to resolve pathfinding. |

---
# Modules

## Available {% include lk id='🝓 Goal Pickers' %} modules
<br>
{% include card_any tagged="goalpicker" %}

---
## Available {% include lk id='🝰 Heuristics' %} modules
<br>
{% include card_any tagged="heuristics" %}

---
## Available {% include lk id='⊚ Search' %} modules
<br>
{% include card_any tagged="search" %}
