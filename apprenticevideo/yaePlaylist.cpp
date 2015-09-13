// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// Created      : Sat Apr  7 23:41:07 MDT 2012
// Copyright    : Pavel Koshevoy
// License      : MIT -- http://www.opensource.org/licenses/mit-license.php

// system includes:
#include <iostream>
#include <algorithm>
#include <limits>

// boost includes:
#include <boost/filesystem.hpp>

// Qt includes:
#include <QCryptographicHash>
#include <QFileInfo>
#include <QUrl>

// yae includes:
#include <yae/utils/yae_utils.h>

// local includes:
#include <yaePlaylist.h>
#include <yaeUtilsQt.h>

// namespace shortcut:
namespace fs = boost::filesystem;


namespace yae
{

  //----------------------------------------------------------------
  // getKeyPathHash
  //
  static std::string
  getKeyPathHash(const std::list<PlaylistKey> & keyPath)
  {
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    for (std::list<PlaylistKey>::const_iterator i = keyPath.begin();
         i != keyPath.end(); ++i)
    {
      const PlaylistKey & key = *i;
      crypto.addData(key.key_.toUtf8());
      crypto.addData(key.ext_.toUtf8());
    }

    std::string groupHash(crypto.result().toHex().constData());
    return groupHash;
  }

  //----------------------------------------------------------------
  // getKeyHash
  //
  static std::string
  getKeyHash(const PlaylistKey & key)
  {
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    crypto.addData(key.key_.toUtf8());
    crypto.addData(key.ext_.toUtf8());

    std::string itemHash(crypto.result().toHex().constData());
    return itemHash;
  }


  //----------------------------------------------------------------
  // PlaylistKey::PlaylistKey
  //
  PlaylistKey::PlaylistKey(const QString & key, const QString & ext):
    key_(key),
    ext_(ext)
  {}

  //----------------------------------------------------------------
  // PlaylistKey::operator
  //
  bool
  PlaylistKey::operator == (const PlaylistKey & k) const
  {
    int diff = key_.compare(k.key_, Qt::CaseInsensitive);
    if (diff)
    {
      return false;
    }

    diff = ext_.compare(k.ext_, Qt::CaseInsensitive);
    return !diff;
  }

  //----------------------------------------------------------------
  // PlaylistKey::operator <
  //
  bool
  PlaylistKey::operator < (const PlaylistKey & k) const
  {
    int diff = key_.compare(k.key_, Qt::CaseInsensitive);
    if (diff)
    {
      return diff < 0;
    }

    diff = ext_.compare(k.ext_, Qt::CaseInsensitive);
    return diff < 0;
  }

  //----------------------------------------------------------------
  // PlaylistKey::operator >
  //
  bool PlaylistKey::operator > (const PlaylistKey & k) const
  {
    int diff = key_.compare(k.key_, Qt::CaseInsensitive);
    if (diff)
    {
      return diff > 0;
    }

    diff = ext_.compare(k.ext_, Qt::CaseInsensitive);
    return diff > 0;
  }

  //----------------------------------------------------------------
  // PlaylistNode::PlaylistNode
  //
  PlaylistNode::PlaylistNode():
    row_(~0)
  {}

  //----------------------------------------------------------------
  // PlaylistNode::PlaylistNode
  //
  PlaylistNode::PlaylistNode(const PlaylistNode & other):
    row_(other.row_)
  {}

  //----------------------------------------------------------------
  // PlaylistNode::~PlaylistNode
  //
  PlaylistNode::~PlaylistNode()
  {}


  //----------------------------------------------------------------
  // PlaylistItem::PlaylistItem
  //
  PlaylistItem::PlaylistItem(PlaylistGroup & group):
    group_(group),
    selected_(false),
    excluded_(false),
    failed_(false)
  {}


  //----------------------------------------------------------------
  // PlaylistGroup::PlaylistGroup
  //
  PlaylistGroup::PlaylistGroup():
    offset_(0),
    collapsed_(false),
    excluded_(false)
  {}


  //----------------------------------------------------------------
  // Playlist::Playlist
  //
  Playlist::Playlist():
    numItems_(0),
    numShown_(0),
    numShownGroups_(0),
    playing_(0),
    current_(0)
  {
    add(std::list<QString>());
  }

  //----------------------------------------------------------------
  // toWords
  //
  static QString
  toWords(const std::list<PlaylistKey> & keys)
  {
    std::list<QString> words;
    for (std::list<PlaylistKey>::const_iterator i = keys.begin();
         i != keys.end(); ++i)
    {
      if (!words.empty())
      {
        // right-pointing double angle bracket:
        words.push_back(QString::fromUtf8(" ""\xc2""\xbb"" "));
      }

      const PlaylistKey & key = *i;
      splitIntoWords(key.key_, words);

      if (!key.ext_.isEmpty())
      {
        words.push_back(key.ext_);
      }
    }

    return toQString(words, true);
  }

  //----------------------------------------------------------------
  // init_lookup
  //
  template <typename TNode>
  static void
  init_lookup(std::map<typename TNode::TKey, std::shared_ptr<TNode> > & lookup,
              const std::vector<std::shared_ptr<TNode> > & nodes)
  {
    typedef std::shared_ptr<TNode> TNodePtr;
    typedef std::vector<std::shared_ptr<TNode> > TNodes;
    typedef typename TNodes::const_iterator TNodesIter;

    lookup.clear();

    for (TNodesIter i = nodes.begin(); i != nodes.end(); ++i)
    {
      const TNodePtr & node = *i;
      lookup[node->key()] = node;
    }
  }

  //----------------------------------------------------------------
  // lookup
  //
  template <typename TNode>
  static std::shared_ptr<TNode>
  lookup(const std::map<typename TNode::TKey, std::shared_ptr<TNode> > & nodes,
         const typename TNode::TKey & key)
  {
    typedef std::shared_ptr<TNode> TNodePtr;
    typedef std::map<typename TNode::TKey, std::shared_ptr<TNode> > TNodeMap;

    typename TNodeMap::const_iterator found = nodes.find(key);
    return (found == nodes.end()) ? TNodePtr() : found->second;
  }

  //----------------------------------------------------------------
  // Playlist::add
  //
  void
  Playlist::add(const std::list<QString> & playlist,

                // optionally pass back a list of hashes for the added items:
                std::list<BookmarkHashInfo> * returnBookmarkHashList,

                // optionally notify an observer about newly added groups:
                TObservePlaylistGroup callbackBeforeAddingGroup,
                TObservePlaylistGroup callbackAfterAddingGroup,
                void * contextAddGroup,

                // optionally notify an observer about newly added items:
                TObservePlaylistItem callbackBeforeAddingItem,
                TObservePlaylistItem callbackAfterAddingItem,
                void * contextAddItem)
  {
#if 0
    std::cerr << "Playlist::add" << std::endl;
#endif

    // a temporary playlist tree used for deciding which of the
    // newly added items should be selected for playback:
    TPlaylistTree tmpTree;

    for (std::list<QString>::const_iterator i = playlist.begin();
         i != playlist.end(); ++i)
    {
      QString path = *i;
      QString humanReadablePath = path;

      QFileInfo fi(path);
      if (fi.exists())
      {
        path = fi.absoluteFilePath();
        humanReadablePath = path;
      }
      else
      {
        QUrl url;

#if YAE_QT4
        url.setEncodedUrl(path.toUtf8(), QUrl::StrictMode);
#elif YAE_QT5
        url.setUrl(path, QUrl::StrictMode);
#endif

        if (url.isValid())
        {
          humanReadablePath = url.toString();
        }
      }

      fi = QFileInfo(humanReadablePath);
      QString name = toWords(fi.completeBaseName());

      if (name.isEmpty())
      {
#if 0
        std::cerr << "IGNORING: " << i->toUtf8().constData() << std::endl;
#endif
        continue;
      }

      // tokenize it, convert into a tree key path:
      std::list<PlaylistKey> keys;
      while (true)
      {
        QString key = fi.fileName();
        if (key.isEmpty())
        {
          break;
        }

        QFileInfo parseKey(key);
        QString base;
        QString ext;

        if (keys.empty())
        {
          base = parseKey.completeBaseName();
          ext = parseKey.suffix();
        }
        else
        {
          base = parseKey.fileName();
        }

        if (keys.empty() && ext.compare(kExtEyetv, Qt::CaseInsensitive) == 0)
        {
          // handle Eye TV archive more gracefully:
          QString program;
          QString episode;
          QString timestamp;
          if (!parseEyetvInfo(path, program, episode, timestamp))
          {
            break;
          }

          if (episode.isEmpty())
          {
            key = timestamp + " " + program;
            keys.push_front(PlaylistKey(key, QString()));
          }
          else
          {
            key = timestamp + " " + episode;
            keys.push_front(PlaylistKey(key, QString()));
          }

          key = program;
          keys.push_front(PlaylistKey(key, QString()));
        }
        else
        {
          key = prepareForSorting(base);
          keys.push_front(PlaylistKey(key, ext));
        }

        QString next = fi.absolutePath();
        fi = QFileInfo(next);
      }

      if (!keys.empty())
      {
        tmpTree.set(keys, path);
        tree_.set(keys, path);
      }
    }

    typedef TPlaylistTree::FringeGroup TFringeGroup;
    typedef std::map<PlaylistKey, QString> TSiblings;

    // lookup the first new item:
    const QString * firstNewItemPath = tmpTree.findFirstFringeItemValue();

    // return hash keys of newly added groups and items:
    if (returnBookmarkHashList)
    {
      std::list<TFringeGroup> fringeGroups;
      tmpTree.get(fringeGroups);

      for (std::list<TFringeGroup>::const_iterator i = fringeGroups.begin();
           i != fringeGroups.end(); ++i)
      {
        // shortcuts:
        const std::list<PlaylistKey> & keyPath = i->fullPath_;
        const TSiblings & siblings = i->siblings_;

        returnBookmarkHashList->push_back(BookmarkHashInfo());
        BookmarkHashInfo & hashInfo = returnBookmarkHashList->back();
        hashInfo.groupHash_ = getKeyPathHash(keyPath);

        for (TSiblings::const_iterator j = siblings.begin();
             j != siblings.end(); ++j)
        {
          const PlaylistKey & key = j->first;
          hashInfo.itemHash_.push_back(getKeyHash(key));
        }
      }
    }

    // flatten the tree into a list of play groups:
    std::list<TFringeGroup> fringeGroups;
    tree_.get(fringeGroups);

    // remove leading redundant keys from abbreviated paths:
    while (!fringeGroups.empty() &&
           isSizeTwoOrMore(fringeGroups.front().abbreviatedPath_))
    {
      const PlaylistKey & head = fringeGroups.front().abbreviatedPath_.front();

      bool same = true;
      std::list<TFringeGroup>::iterator i = fringeGroups.begin();
      for (++i; same && i != fringeGroups.end(); ++i)
      {
        const std::list<PlaylistKey> & abbreviatedPath = i->abbreviatedPath_;
        const PlaylistKey & key = abbreviatedPath.front();
        same = isSizeTwoOrMore(abbreviatedPath) && (key == head);
      }

      if (!same)
      {
        break;
      }

      // remove the head of abbreviated path of each group:
      for (i = fringeGroups.begin(); i != fringeGroups.end(); ++i)
      {
        i->abbreviatedPath_.pop_front();
      }
    }

    // setup a map for fast group lookup:
    std::map<PlaylistGroup::TKey, TPlaylistGroupPtr> groupMap;
    init_lookup(groupMap, groups_);

    // update the group vector:
    numItems_ = 0;
    std::size_t groupCount = 0;

    std::vector<TPlaylistGroupPtr> groupVec;
    for (std::list<TFringeGroup>::const_iterator i = fringeGroups.begin();
         i != fringeGroups.end(); ++i)
    {
      // shortcut:
      const TFringeGroup & fringeGroup = *i;

      TPlaylistGroupPtr groupPtr =
        yae::lookup(groupMap, fringeGroup.fullPath_);
      bool newGroup = !groupPtr;

      if (newGroup)
      {
        if (callbackBeforeAddingGroup)
        {
          callbackBeforeAddingGroup(contextAddGroup, groupCount);
        }

        groupPtr.reset(new PlaylistGroup());
        groupMap[fringeGroup.fullPath_] = groupPtr;
        groups_.insert(groups_.begin() + groupCount, groupPtr);
      }

      PlaylistGroup & group = *groupPtr;
      group.offset_ = numItems_;
      group.row_ = groupCount;
      groupCount++;

      if (newGroup)
      {
        group.keyPath_ = fringeGroup.fullPath_;
        group.name_ = toWords(fringeGroup.abbreviatedPath_);
        group.hash_ = getKeyPathHash(group.keyPath_);

        if (callbackAfterAddingGroup)
        {
          callbackAfterAddingGroup(contextAddGroup, group.row_);
        }
     }

      // setup a map for fast item lookup:
      std::map<PlaylistItem::TKey, TPlaylistItemPtr> itemMap;
      init_lookup(itemMap, group.items_);

      // update the items vector:
      std::size_t groupSize = 0;

      // shortcuts:
      const TSiblings & siblings = fringeGroup.siblings_;

      for (TSiblings::const_iterator j = siblings.begin();
           j != siblings.end(); ++j, ++numItems_)
      {
        const PlaylistKey & key = j->first;
        const QString & value = j->second;

        TPlaylistItemPtr itemPtr = yae::lookup(itemMap, key);
        bool newItem = !itemPtr;

        if (newItem)
        {
          if (callbackBeforeAddingItem)
          {
            callbackBeforeAddingItem(contextAddItem, group.row_, groupSize);
          }

          itemPtr.reset(new PlaylistItem(group));
          itemMap[key] = itemPtr;
          group.items_.insert(group.items_.begin() + groupSize, itemPtr);
        }

        PlaylistItem & item = *itemPtr;
        item.row_ = groupSize;
        groupSize++;

        if (newItem)
        {
          item.key_ = key;
          item.path_ = value;
          item.name_ = toWords(key.key_);
          item.ext_ = key.ext_;
          item.hash_ = getKeyHash(item.key_);
        }

        if (firstNewItemPath && *firstNewItemPath == item.path_)
        {
          current_ = numItems_;
        }

        if (newItem && callbackAfterAddingItem)
        {
          callbackAfterAddingItem(contextAddItem, group.row_, item.row_);
        }
      }
    }

    if (applyFilter())
    {
      current_ = closestItem(current_);
    }

    updateOffsets();
    setPlayingItem(current_, true);

#if 0
    for (std::size_t i = 0; i < groups_.size(); i++)
    {
      const PlaylistGroup & group = *(groups_[i]);
      std::cerr
        << "\ngroup " << i
        << ", row: " << group.row_
        << ", offset: " << group.offset_
        << ", size: " << group.items_.size()
        << std::endl;

      for (std::size_t j = 0; j < group.items_.size(); j++)
      {
        const PlaylistItem & item = *(group.items_[j]);
        std::cerr
          << "  item " << j
          << ", row: " << item.row_
          << std::endl;
      }
    }
#endif
  }

  //----------------------------------------------------------------
  // Playlist::playingItem
  //
  std::size_t
  Playlist::playingItem() const
  {
    return playing_;
  }

  //----------------------------------------------------------------
  // Playlist::countItems
  //
  std::size_t
  Playlist::countItems() const
  {
    return numItems_;
  }

  //----------------------------------------------------------------
  // Playlist::countItemsAhead
  //
  std::size_t
  Playlist::countItemsAhead() const
  {
    return (playing_ < numItems_) ? (numItems_ - playing_) : 0;
  }

  //----------------------------------------------------------------
  // Playlist::countItemsBehind
  //
  std::size_t
  Playlist::countItemsBehind() const
  {
    return (playing_ < numItems_) ? playing_ : numItems_;
  }

  //----------------------------------------------------------------
  // Playlist::closestGroup
  //
  TPlaylistGroupPtr
  Playlist::closestGroup(std::size_t index,
                         Playlist::TDirection where) const
  {
    if (numItems_ == numShown_)
    {
      // no items have been excluded:
      return lookupGroup(index);
    }

    TPlaylistGroupPtr prev;

    for (std::vector<TPlaylistGroupPtr>::const_iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      const TPlaylistGroupPtr & groupPtr = *i;
      const PlaylistGroup & group = *groupPtr;

      if (group.excluded_)
      {
        continue;
      }

      std::size_t groupSize = group.items_.size();
      std::size_t groupEnd = group.offset_ + groupSize;
      if (groupEnd <= index)
      {
        prev = groupPtr;
      }

      if (index < groupEnd)
      {
        if (index >= group.offset_)
        {
          // make sure the group has an un-excluded item
          // in the range that we are interested in:
          const int step = where == kAhead ? 1 : -1;

          for (std::size_t j = index - group.offset_; j < groupSize; j += step)
          {
            const PlaylistItem & item = *(group.items_[j]);
            if (!item.excluded_)
            {
              return groupPtr;
            }
          }
        }
        else if (where == kAhead)
        {
          return groupPtr;
        }

        if (where == kBehind)
        {
          break;
        }
      }
    }

    if (where == kBehind)
    {
      return prev;
    }

    return TPlaylistGroupPtr();
  }

  //----------------------------------------------------------------
  // Playlist::closestItem
  //
  std::size_t
  Playlist::closestItem(std::size_t index,
                        Playlist::TDirection where,
                        TPlaylistGroupPtr * returnGroup) const
  {
    if (numItems_ == numShown_)
    {
      // no items have been excluded:
      if (returnGroup)
      {
        *returnGroup = lookupGroup(index);
      }

      return index;
    }

    TPlaylistGroupPtr group = closestGroup(index, where);
    if (returnGroup)
    {
      *returnGroup = group;
    }

    if (!group)
    {
      if (where == kAhead)
      {
        return numItems_;
      }

      // nothing left behind, try looking ahead for the first un-excluded item:
      return closestItem(index, kAhead, returnGroup);
    }

    // find the closest item within this group:
    const std::vector<TPlaylistItemPtr> & items = group->items_;
    const std::size_t groupSize = items.size();
    const int step = where == kAhead ? 1 : -1;

    std::size_t i =
      index < group->offset_ ? 0 :
      index - group->offset_ >= groupSize ? groupSize - 1 :
      index - group->offset_;

    for (; i < groupSize; i += step)
    {
      const PlaylistItem & item = *(items[i]);
      if (!item.excluded_)
      {
        return (group->offset_ + i);
      }
    }

    if (where == kAhead)
    {
      return numItems_;
    }

    // nothing left behind, try looking ahead for the first un-excluded item:
    return closestItem(index, kAhead, returnGroup);
  }

  //----------------------------------------------------------------
  // keywordsMatch
  //
  static bool
  keywordsMatch(const std::list<QString> & keywords, const QString  & text)
  {
    for (std::list<QString>::const_iterator i = keywords.begin();
         i != keywords.end(); ++i)
    {
      const QString & keyword = *i;
      if (!text.contains(keyword, Qt::CaseInsensitive))
      {
        return false;
      }
    }

#if 0
    std::cerr << "KEYWORDS MATCH: " << text.toUtf8().constData() << std::endl;
#endif
    return true;
  }

  //----------------------------------------------------------------
  // Playlist::filterChanged
  //
  bool
  Playlist::filterChanged(const QString & filter)
  {
    keywords_.clear();
    splitIntoWords(filter, keywords_);

    if (applyFilter())
    {
      updateOffsets();
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------
  // Playlist::applyFilter
  //
  bool
  Playlist::applyFilter()
  {
    bool exclude = !keywords_.empty();
    bool changed = false;
    std::size_t index = 0;

    for (std::vector<TPlaylistGroupPtr>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *(*i);

      std::size_t groupSize = group.items_.size();
      std::size_t numExcluded = 0;

      for (std::vector<TPlaylistItemPtr>::iterator j = group.items_.begin();
           j != group.items_.end(); ++j, index++)
      {
        PlaylistItem & item = *(*j);

        if (!exclude)
        {
          if (item.excluded_)
          {
            item.excluded_ = false;
            changed = true;
          }

          continue;
        }

        QString text =
          group.name_ + QString::fromUtf8(" ") +
          item.name_ + QString::fromUtf8(".") +
          item.ext_;

        if (index == playing_)
        {
          text += QObject::tr("NOW PLAYING");
        }

        if (!keywordsMatch(keywords_, text))
        {
          if (!item.excluded_)
          {
            item.excluded_ = true;
            changed = true;
          }

          numExcluded++;
        }
        else if (item.excluded_)
        {
          item.excluded_ = false;
          changed = true;
        }
      }

      if (!group.keyPath_.empty())
      {
        group.excluded_ = (groupSize == numExcluded);
      }
    }

    return changed;
  }

  //----------------------------------------------------------------
  // Playlist::setPlayingItem
  //
  void
  Playlist::setPlayingItem(std::size_t index, bool force)
  {
#if 0
    std::cerr << "Playlist::setPlayingItem" << std::endl;
#endif

    if (index != playing_ || force)
    {
      playing_ = (index < numItems_) ? index : numItems_;
      current_ = playing_;
      selectItem(playing_);
    }
  }

  //----------------------------------------------------------------
  // Playlist::selectAll
  //
  void
  Playlist::selectAll()
  {
    for (std::vector<TPlaylistGroupPtr>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *(*i);
      if (group.excluded_)
      {
        continue;
      }

      selectGroup(group);
    }
  }

  //----------------------------------------------------------------
  // Playlist::selectGroup
  //
  void
  Playlist::selectGroup(PlaylistGroup & group)
  {
    for (std::vector<TPlaylistItemPtr>::iterator i = group.items_.begin();
         i != group.items_.end(); ++i)
    {
      PlaylistItem & item = *(*i);
      if (item.excluded_)
      {
        continue;
      }

      item.selected_ = true;
    }
  }

  //----------------------------------------------------------------
  // Playlist::selectItem
  //
  void
  Playlist::selectItem(std::size_t indexSel, bool exclusive)
  {
#if 0
    std::cerr << "Playlist::selectItem: " << indexSel << std::endl;
#endif
    bool itemSelected = false;

    for (std::vector<TPlaylistGroupPtr>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *(*i);
      if (group.excluded_)
      {
        continue;
      }

      std::size_t groupEnd = group.offset_ + group.items_.size();

      if (exclusive)
      {
        for (std::vector<TPlaylistItemPtr>::iterator j = group.items_.begin();
             j != group.items_.end(); ++j)
        {
          PlaylistItem & item = *(*j);
          if (item.excluded_)
          {
            continue;
          }

          item.selected_ = false;
        }
      }

      if (group.offset_ <= indexSel && indexSel < groupEnd)
      {
        PlaylistItem & item = *(group.items_[indexSel - group.offset_]);
        item.selected_ = true;
        itemSelected = true;

        if (!exclusive)
        {
          // done:
          break;
        }
      }
    }

    YAE_ASSERT(itemSelected || indexSel == numItems_);
  }

  //----------------------------------------------------------------
  // Playlist::removeSelected
  //
  void
  Playlist::removeSelected()
  {
#if 0
    std::cerr << "Playlist::removeSelected" << std::endl;
#endif

    std::size_t oldIndex = 0;
    std::size_t newIndex = 0;
    std::size_t newPlaying = playing_;
    bool playingRemoved = false;

    for (std::vector<TPlaylistGroupPtr>::iterator i = groups_.begin();
         i != groups_.end(); )
    {
      PlaylistGroup & group = *(*i);

      if (group.excluded_)
      {
        std::size_t groupSize = group.items_.size();
        oldIndex += groupSize;
        newIndex += groupSize;
        ++i;
        continue;
      }

      for (std::vector<TPlaylistItemPtr>::iterator j = group.items_.begin();
           j != group.items_.end(); oldIndex++)
      {
        PlaylistItem & item = *(*j);

        if (item.excluded_ || !item.selected_)
        {
          ++j;
          newIndex++;
          continue;
        }

        if (oldIndex < playing_)
        {
          // adjust the playing index:
          newPlaying--;
        }
        else if (oldIndex == playing_)
        {
          // playing item has changed:
          playingRemoved = true;
        }

        current_ = newIndex;

        // 1. remove the item from the tree:
        std::list<PlaylistKey> keyPath = group.keyPath_;
        keyPath.push_back(item.key_);
        tree_.remove(keyPath);

        // 2. remove the item from the group:
        j = group.items_.erase(j);
      }

      // if the group is empty and has a key path, remove it:
      if (!group.items_.empty() || group.keyPath_.empty() || group.excluded_)
      {
        ++i;
        continue;
      }

      i = groups_.erase(i);
    }

    updateOffsets();

    if (current_ >= numItems_)
    {
      current_ = numItems_ ? numItems_ - 1 : 0;
    }

    // must account for the excluded items:
    current_ = closestItem(current_, kBehind);

    if (current_ < numItems_)
    {
      TPlaylistItemPtr item = lookup(current_);
      item->selected_ = true;
    }

    if (playingRemoved)
    {
      setPlayingItem(current_, true);
    }
    else
    {
      playing_ = newPlaying;
      current_ = playing_;
    }
  }

  //----------------------------------------------------------------
  // Playlist::removeItems
  //
  void
  Playlist::removeItems(std::size_t groupIndex, std::size_t itemIndex)
  {
    bool playingRemoved = false;
    std::size_t newPlaying = playing_;

    PlaylistGroup & group = *(groups_[groupIndex]);
    if (group.excluded_)
    {
      YAE_ASSERT(false);
      return;
    }

    if (itemIndex < numItems_)
    {
      // remove one item:
      std::vector<TPlaylistItemPtr>::iterator iter =
        group.items_.begin() + (itemIndex - group.offset_);

      // remove item from the tree:
      {
        PlaylistItem & item = *(*iter);
        std::list<PlaylistKey> keyPath = group.keyPath_;
        keyPath.push_back(item.key_);
        tree_.remove(keyPath);
      }

      if (itemIndex < playing_)
      {
        // adjust the playing index:
        newPlaying = playing_ - 1;
      }
      else if (itemIndex == playing_)
      {
        // playing item has changed:
        playingRemoved = true;
        newPlaying = playing_;
      }

      if (itemIndex < current_)
      {
        current_--;
      }

      group.items_.erase(iter);
    }
    else
    {
      // remove entire group:
      for (std::vector<TPlaylistItemPtr>::iterator iter = group.items_.begin();
           iter != group.items_.end(); ++iter)
      {
        // remove item from the tree:
        PlaylistItem & item = *(*iter);
        std::list<PlaylistKey> keyPath = group.keyPath_;
        keyPath.push_back(item.key_);
        tree_.remove(keyPath);
      }

      std::size_t groupSize = group.items_.size();
      std::size_t groupEnd = group.offset_ + groupSize;
      if (groupEnd < playing_)
      {
        // adjust the playing index:
        newPlaying = playing_ - groupSize;
      }
      else if (group.offset_ <= playing_)
      {
        // playing item has changed:
        playingRemoved = true;
        newPlaying = group.offset_;
      }

      if (groupEnd < current_)
      {
        current_ -= groupSize;
      }
      else if (group.offset_ <= current_)
      {
        current_ = group.offset_;
      }

      group.items_.clear();
    }

    // if the group is empty and has a key path, remove it:
    if (group.items_.empty() && !group.keyPath_.empty())
    {
      std::vector<TPlaylistGroupPtr>::iterator
        iter = groups_.begin() + groupIndex;
      groups_.erase(iter);
    }

    updateOffsets();

    if (newPlaying >= numItems_)
    {
      newPlaying = numItems_ ? numItems_ - 1 : 0;
    }

    if (current_ >= numItems_)
    {
      current_ = numItems_ ? numItems_ - 1 : 0;
    }

    // must account for the excluded items:
    newPlaying = closestItem(newPlaying, kBehind);
    current_ = closestItem(current_, kBehind);

    if (current_ < numItems_)
    {
      TPlaylistItemPtr item = lookup(current_);
      item->selected_ = true;
    }

    if (playingRemoved)
    {
      setPlayingItem(newPlaying, true);
    }
    else
    {
      playing_ = newPlaying;
      current_ = playing_;
    }
  }

  //----------------------------------------------------------------
  // Playlist::setCurrentItem
  //
  bool
  Playlist::setCurrentItem(int groupRow, int itemRow)
  {
    TPlaylistGroupPtr group;
    TPlaylistItemPtr item = lookup(group, groupRow, itemRow);

    std::size_t index = 0;

    if (group)
    {
      index += group->offset_;
    }

    if (item)
    {
      index += item->row_;
    }

    index = closestItem(index);

    if (index == current_)
    {
      return false;
    }

    current_ = index;
    return true;
  }

  //----------------------------------------------------------------
  // lookupLastGroupIndex
  //
  // return index of the last non-excluded group:
  //
  static std::size_t
  lookupLastGroupIndex(const std::vector<TPlaylistGroupPtr> & groups)
  {
    std::size_t index = groups.size();
    for (std::vector<TPlaylistGroupPtr>::const_reverse_iterator
           i = groups.rbegin(); i != groups.rend(); ++i, --index)
    {
      const PlaylistGroup & group = *(*i);
      if (!group.excluded_)
      {
#if 0
        std::cerr << "lookupGroup, last: "
                  << group.name_.toUtf8().constData()
                  << std::endl;
#endif
        return index - 1;
      }
    }

    return groups.size();
  }

  //----------------------------------------------------------------
  // lookupLastGroup
  //
  inline static TPlaylistGroupPtr
  lookupLastGroup(const std::vector<TPlaylistGroupPtr> & groups)
  {
    std::size_t numGroups = groups.size();
    std::size_t i = lookupLastGroupIndex(groups);

    TPlaylistGroupPtr found;

    if (i < numGroups)
    {
      found = groups[i];
    }

    return found;
  }

  //----------------------------------------------------------------
  // Playlist::lookupGroup
  //
  TPlaylistGroupPtr
  Playlist::lookupGroup(std::size_t index) const
  {
#if 0
    std::cerr << "Playlist::lookupGroup: " << index << std::endl;
#endif

    if (groups_.empty())
    {
      return NULL;
    }

    if (index >= numItems_)
    {
      YAE_ASSERT(index == numItems_);
      return NULL;
    }

    const std::size_t numGroups = groups_.size();
    std::size_t i0 = 0;
    std::size_t i1 = numGroups;

    while (i0 != i1)
    {
      std::size_t i = i0 + (i1 - i0) / 2;

      const PlaylistGroup & group = *(groups_[i]);
      std::size_t numItems = group.items_.size();
      std::size_t groupEnd = group.offset_ + numItems;

      if (index < groupEnd)
      {
        i1 = std::min<std::size_t>(i, i1 - 1);
      }
      else
      {
        i0 = std::max<std::size_t>(i, i0 + 1);
      }
    }

    if (i0 < numGroups)
    {
      const TPlaylistGroupPtr & group = groups_[i0];
      std::size_t numItems = group->items_.size();
      std::size_t groupEnd = group->offset_ + numItems;

      if (index < groupEnd)
      {
        return group;
      }
    }

    YAE_ASSERT(false);
    return lookupLastGroup(groups_);
  }

  //----------------------------------------------------------------
  // Playlist::lookup
  //
  TPlaylistItemPtr
  Playlist::lookup(std::size_t index, TPlaylistGroupPtr * returnGroup) const
  {
#if 0
    std::cerr << "Playlist::lookup: " << index << std::endl;
#endif

    TPlaylistGroupPtr group = lookupGroup(index);
    if (returnGroup)
    {
      *returnGroup = group;
    }

    if (group)
    {
      std::size_t groupSize = group->items_.size();
      std::size_t i = index - group->offset_;

      YAE_ASSERT(i < groupSize || index == numItems_);

      return
        (i < groupSize) ?
        group->items_[i] :
        TPlaylistItemPtr();
    }

    return TPlaylistItemPtr();
  }

  //----------------------------------------------------------------
  // Playlist::lookupGroup
  //
  TPlaylistGroupPtr
  Playlist::lookupGroup(const std::string & groupHash) const
  {
    if (groupHash.empty())
    {
      return TPlaylistGroupPtr();
    }

    const std::size_t numGroups = groups_.size();
    for (std::size_t i = 0; i < numGroups; i++)
    {
      const TPlaylistGroupPtr & group = groups_[i];
      if (groupHash == group->hash_)
      {
        return group;
      }
    }

    return TPlaylistGroupPtr();
  }

  //----------------------------------------------------------------
  // Playlist::lookup
  //
  TPlaylistItemPtr
  Playlist::lookup(const std::string & groupHash,
                   const std::string & itemHash,
                   std::size_t * returnItemIndex,
                   TPlaylistGroupPtr * returnGroup) const
  {
    TPlaylistGroupPtr group = lookupGroup(groupHash);
    if (!group || itemHash.empty())
    {
      return TPlaylistItemPtr();
    }

    if (returnGroup)
    {
      *returnGroup = group;
    }

    std::size_t groupSize = group->items_.size();
    for (std::size_t i = 0; i < groupSize; i++)
    {
      const TPlaylistItemPtr & item = group->items_[i];
      if (itemHash == item->hash_)
      {
        if (*returnItemIndex)
        {
          *returnItemIndex = group->offset_ + i;
        }

        return item;
      }
    }

    if (*returnItemIndex)
    {
      *returnItemIndex = std::numeric_limits<std::size_t>::max();
    }

    return TPlaylistItemPtr();
  }

  //----------------------------------------------------------------
  // Playlist::lookup
  //
  TPlaylistItemPtr
  Playlist::lookup(TPlaylistGroupPtr & group, int groupRow, int itemRow) const
  {
    if (groupRow < 0 || groupRow >= groups_.size())
    {
      group = TPlaylistGroupPtr();
      return TPlaylistItemPtr();
    }

    group = groups_[groupRow];
    if (itemRow < 0 || itemRow >= group->items_.size())
    {
      return TPlaylistItemPtr();
    }

    const TPlaylistItemPtr & item = group->items_[itemRow];
    return item;
  }

  //----------------------------------------------------------------
  // Playlist::updateOffsets
  //
  void
  Playlist::updateOffsets()
  {
#if 0
    std::cerr << "Playlist::updateOffsets" << std::endl;
#endif

    std::size_t offset = 0;
    numShown_ = 0;
    numShownGroups_ = 0;

    for (std::vector<TPlaylistGroupPtr>::iterator i = groups_.begin();
         i != groups_.end(); ++i)
    {
      PlaylistGroup & group = *(*i);
      group.offset_ = offset;

      if (!group.excluded_)
      {
        numShownGroups_++;
      }

      for (std::vector<TPlaylistItemPtr>::iterator j = group.items_.begin();
           j != group.items_.end(); ++j)
      {
        PlaylistItem & item = *(*j);

        if (!item.excluded_)
        {
          numShown_++;
        }
      }

      offset += group.items_.size();
    }

    numItems_ = offset;
  }
}
