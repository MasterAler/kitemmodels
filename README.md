# What is this?

**KItemModels** is a set of item models extending the Qt model-view framework.

This is a fork of [KItemModels](https://github.com/KDE/kitemmodels) library, with same sources as were provided by **KDE**. I don't own them, all the copyrights are in their places, LGPL holds as it was, of course.

## Why forking?

KItemModels is awesome, it contains several classes that can help a lot in building a truly advanced GUI, but it's been severely tied to the CMake and all that sophisticated build concept that the authors had seemingly been using in their everyday job. You can't just "take and use it", build is troubled on **Windows** and even on Linux **CMake** build is not all that straightforward and portable.

That goes with two more facts:
1. Using **LGPL** library in any proprietary project (meaning your current job project) is possible via the shared library only, no "copying the sources as-is", which means a nessesity to have a convenient way to build it.
2. Those who prefer **QMake** (a lot of crossplatform developers) would have gotten pain trying to use this library without some adaptation 

## What can be found here?

Solution to the inconveniences mentioned above is pretty simple and here it is, a **QMake** version of the library. It is a subdirectory project, that can be built by Qt Creator on both **Windows & Linux**. Even most of the autotests are present, they are not of much need really, so I've thrown away those that could not be launched without too much effort.

Yeah, that was not too difficult, but now this tool can become immediately useful in any Qt project.

### Description

KItemModels provides the following models:

* KBreadcrumbSelectionModel - Selects the parents of selected items to create
  breadcrumbs
* KCheckableProxyModel - Adds a checkable capability to a source model
* KConcatenateRowsProxyModel - Concatenates rows from multiple source models
* KDescendantsProxyModel - Proxy Model for restructuring a Tree into a list
* KExtraColumnsProxyModel - Adds columns after existing columns
* KLinkItemSelectionModel - Share a selection in multiple views which do not
  have the same source model
* KModelIndexProxyMapper - Mapping of indexes and selections through proxy
  models
* KNumberModel - Creates a model of entries from N to M with rows at a given interval
* KRearrangeColumnsProxyModel - Can reorder and hide columns from the source model
* KRecursiveFilterProxyModel - Recursive filtering of models
* KSelectionProxyModel - A Proxy Model which presents a subset of its source
  model to observers


