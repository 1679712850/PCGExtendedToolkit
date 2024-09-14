---
layout: page
grand_parent: Staging
parent: Asset Collection
title: 🝱 Actor Collection
subtitle: Actor Collection DataAsset
summary: The **Actor Collection** DataAsset is a list of Actors, that comes with all the Asset Collection goodies.
color: white
splash: icons/icon_custom-graphs-params.svg
see_also: 
    - Asset Collection
tagged: 
    - assetcollection
    - staging
nav_order: 7
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Entries           | Entries are a list of actors (soft) references and their associated shared properties.<br>*You really just need to set the Actor property for it to work.* |
| Do Not Ignore Invalid Entries          | Forces distribution of this to NOT skip over invalid entries.<br>This can be useful to create 'weighted' spaces, and can be overriden on a per-node basis. |

|: **Advanced** :||
| Auto Rebuild Staging           | Enabled by default.<br>When enabled, any user-made modification to the asset collection will trigger a rebuilding of the staging data *(saves you a click, in case your forget about it.)* |

{% include embed id='settings-col-asset' %}