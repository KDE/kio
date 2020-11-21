/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2005-2007 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
// $Id: kacl.cpp 424977 2005-06-13 15:13:22Z tilladam $

#include "kacl.h"

#include <config-kiocore.h>

#if HAVE_POSIX_ACL
#include <sys/acl.h>
#include <acl/libacl.h>
#endif
#include <QHash>
#include <QString>
#include <QDataStream>


#include <memory>

#include "kiocoredebug.h"

class Q_DECL_HIDDEN KACL::KACLPrivate
{
public:
    KACLPrivate()
#if HAVE_POSIX_ACL
        : m_acl(nullptr)
#endif
    {}
#if HAVE_POSIX_ACL
    explicit KACLPrivate(acl_t acl)
        : m_acl(acl) {}
#endif
#if HAVE_POSIX_ACL
    ~KACLPrivate()
    {
        if (m_acl) {
            acl_free(m_acl);
        }
    }
#endif
    // helpers
#if HAVE_POSIX_ACL
    bool setMaskPermissions(unsigned short v);
    QString getUserName(uid_t uid) const;
    QString getGroupName(gid_t gid) const;
    bool setAllUsersOrGroups(const QList< QPair<QString, unsigned short> > &list, acl_tag_t type);
    bool setNamedUserOrGroupPermissions(const QString &name, unsigned short permissions, acl_tag_t type);

    acl_t m_acl;
    mutable QHash<uid_t, QString> m_usercache;
    mutable QHash<gid_t, QString> m_groupcache;
#endif
};

KACL::KACL(const QString &aclString)
    : d(new KACLPrivate)
{
    setACL(aclString);
}

KACL::KACL(mode_t basePermissions)
#if HAVE_POSIX_ACL
    : d(new KACLPrivate(acl_from_mode(basePermissions)))
#else
    : d(new KACLPrivate)
#endif
{
#if !HAVE_POSIX_ACL
    Q_UNUSED(basePermissions);
#endif
}

KACL::KACL()
    : d(new KACLPrivate)
{
}

KACL::KACL(const KACL &rhs)
    : d(new KACLPrivate)
{
    setACL(rhs.asString());
}

KACL::~KACL()
{
    delete d;
}

KACL &KACL::operator=(const KACL &rhs)
{
    if (this != &rhs) {
        setACL(rhs.asString());
    }
    return *this;
}

bool KACL::operator==(const KACL &rhs) const
{
#if HAVE_POSIX_ACL
    return (acl_cmp(d->m_acl, rhs.d->m_acl) == 0);
#else
    Q_UNUSED(rhs);
    return true;
#endif
}

bool KACL::operator!=(const KACL &rhs) const
{
    return !operator==(rhs);
}

bool KACL::isValid() const
{
    bool valid = false;
#if HAVE_POSIX_ACL
    if (d->m_acl) {
        valid = (acl_valid(d->m_acl) == 0);
    }
#endif
    return valid;
}

bool KACL::isExtended() const
{
#if HAVE_POSIX_ACL
    return (acl_equiv_mode(d->m_acl, nullptr) != 0);
#else
    return false;
#endif
}

#if HAVE_POSIX_ACL
static acl_entry_t entryForTag(acl_t acl, acl_tag_t tag)
{
    acl_entry_t entry;
    int ret = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag == tag) {
            return entry;
        }
        ret = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
    }
    return nullptr;
}

static unsigned short entryToPermissions(acl_entry_t entry)
{
    if (entry == nullptr) {
        return 0;
    }
    acl_permset_t permset;
    if (acl_get_permset(entry, &permset) != 0) {
        return 0;
    }
    return (acl_get_perm(permset, ACL_READ) << 2 |
            acl_get_perm(permset, ACL_WRITE) << 1 |
            acl_get_perm(permset, ACL_EXECUTE));
}

static void permissionsToEntry(acl_entry_t entry, unsigned short v)
{
    if (entry == nullptr) {
        return;
    }
    acl_permset_t permset;
    if (acl_get_permset(entry, &permset) != 0) {
        return;
    }
    acl_clear_perms(permset);
    if (v & 4) {
        acl_add_perm(permset, ACL_READ);
    }
    if (v & 2) {
        acl_add_perm(permset, ACL_WRITE);
    }
    if (v & 1) {
        acl_add_perm(permset, ACL_EXECUTE);
    }
}

static int getUidForName(const QString &name)
{
    struct passwd *user = getpwnam(name.toLocal8Bit().constData());
    if (user) {
        return user->pw_uid;
    } else {
        return -1;
    }
}

static int getGidForName(const QString &name)
{
    struct group *group = getgrnam(name.toLocal8Bit().constData());
    if (group) {
        return group->gr_gid;
    } else {
        return -1;
    }
}
#endif
// ------------------ begin API implementation ------------

unsigned short KACL::ownerPermissions() const
{
#if HAVE_POSIX_ACL
    return entryToPermissions(entryForTag(d->m_acl, ACL_USER_OBJ));
#else
    return 0;
#endif
}

bool KACL::setOwnerPermissions(unsigned short v)
{
#if HAVE_POSIX_ACL
    permissionsToEntry(entryForTag(d->m_acl, ACL_USER_OBJ), v);
#else
    Q_UNUSED(v);
#endif
    return true;
}

unsigned short KACL::owningGroupPermissions() const
{
#if HAVE_POSIX_ACL
    return entryToPermissions(entryForTag(d->m_acl, ACL_GROUP_OBJ));
#else
    return 0;
#endif
}

bool KACL::setOwningGroupPermissions(unsigned short v)
{
#if HAVE_POSIX_ACL
    permissionsToEntry(entryForTag(d->m_acl, ACL_GROUP_OBJ), v);
#else
    Q_UNUSED(v);
#endif
    return true;
}

unsigned short KACL::othersPermissions() const
{
#if HAVE_POSIX_ACL
    return entryToPermissions(entryForTag(d->m_acl, ACL_OTHER));
#else
    return 0;
#endif
}

bool KACL::setOthersPermissions(unsigned short v)
{
#if HAVE_POSIX_ACL
    permissionsToEntry(entryForTag(d->m_acl, ACL_OTHER), v);
#else
    Q_UNUSED(v);
#endif
    return true;
}

mode_t KACL::basePermissions() const
{
    mode_t perms(0);
#if HAVE_POSIX_ACL
    if (ownerPermissions() & ACL_READ) {
        perms |= S_IRUSR;
    }
    if (ownerPermissions() & ACL_WRITE) {
        perms |= S_IWUSR;
    }
    if (ownerPermissions() & ACL_EXECUTE) {
        perms |= S_IXUSR;
    }
    if (owningGroupPermissions() & ACL_READ) {
        perms |= S_IRGRP;
    }
    if (owningGroupPermissions() & ACL_WRITE) {
        perms |= S_IWGRP;
    }
    if (owningGroupPermissions() & ACL_EXECUTE) {
        perms |= S_IXGRP;
    }
    if (othersPermissions() & ACL_READ) {
        perms |= S_IROTH;
    }
    if (othersPermissions() & ACL_WRITE) {
        perms |= S_IWOTH;
    }
    if (othersPermissions() & ACL_EXECUTE) {
        perms |= S_IXOTH;
    }
#endif
    return perms;
}

unsigned short KACL::maskPermissions(bool &exists) const
{
    exists = true;
#if HAVE_POSIX_ACL
    acl_entry_t entry = entryForTag(d->m_acl, ACL_MASK);
    if (entry == nullptr) {
        exists = false;
        return 0;
    }
    return entryToPermissions(entry);
#else
    return 0;
#endif
}

#if HAVE_POSIX_ACL
bool KACL::KACLPrivate::setMaskPermissions(unsigned short v)
{
    acl_entry_t entry = entryForTag(m_acl, ACL_MASK);
    if (entry == nullptr) {
        acl_create_entry(&m_acl, &entry);
        acl_set_tag_type(entry, ACL_MASK);
    }
    permissionsToEntry(entry, v);
    return true;
}
#endif

bool KACL::setMaskPermissions(unsigned short v)
{
#if HAVE_POSIX_ACL
    return d->setMaskPermissions(v);
#else
    Q_UNUSED(v);
    return true;
#endif
}

#if HAVE_POSIX_ACL
using unique_ptr_acl_free = std::unique_ptr<void, int(*)(void*)>;
#endif

/**************************
 * Deal with named users  *
 **************************/
unsigned short KACL::namedUserPermissions(const QString &name, bool *exists) const
{
#if HAVE_POSIX_ACL
    acl_entry_t entry;
    *exists = false;
    int ret = acl_get_entry(d->m_acl, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag ==  ACL_USER) {
            const unique_ptr_acl_free idptr(acl_get_qualifier(entry), acl_free);
            const uid_t id = *(static_cast<uid_t *>(idptr.get()));
            if (d->getUserName(id) == name) {
                *exists = true;
                return entryToPermissions(entry);
            }
        }
        ret = acl_get_entry(d->m_acl, ACL_NEXT_ENTRY, &entry);
    }
#else
    Q_UNUSED(name);
    Q_UNUSED(exists);
#endif
    return 0;
}

#if HAVE_POSIX_ACL
bool KACL::KACLPrivate::setNamedUserOrGroupPermissions(const QString &name, unsigned short permissions, acl_tag_t type)
{
    bool allIsWell = true;
    acl_t newACL = acl_dup(m_acl);
    acl_entry_t entry;
    bool createdNewEntry = false;
    bool found = false;
    int ret = acl_get_entry(newACL, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag == type) {
            const unique_ptr_acl_free idptr(acl_get_qualifier(entry), acl_free);
            const int id = * (static_cast<int *>(idptr.get()));  // We assume that sizeof(uid_t) == sizeof(gid_t)
            const QString entryName = type == ACL_USER ? getUserName(id) : getGroupName(id);
            if (entryName == name) {
                // found him, update
                permissionsToEntry(entry, permissions);
                found = true;
                break;
            }
        }
        ret = acl_get_entry(newACL, ACL_NEXT_ENTRY, &entry);
    }
    if (!found) {
        acl_create_entry(&newACL, &entry);
        acl_set_tag_type(entry, type);
        int id = type == ACL_USER ? getUidForName(name) : getGidForName(name);
        if (id == -1 ||  acl_set_qualifier(entry, &id) != 0) {
            acl_delete_entry(newACL, entry);
            allIsWell = false;
        } else {
            permissionsToEntry(entry, permissions);
            createdNewEntry = true;
        }
    }
    if (allIsWell && createdNewEntry) {
        // 23.1.1 of 1003.1e states that as soon as there is a named user or
        // named group entry, there needs to be a mask entry as well, so add
        // one, if the user hasn't explicitly set one.
        if (entryForTag(newACL, ACL_MASK) == nullptr) {
            acl_calc_mask(&newACL);
        }
    }

    if (!allIsWell || acl_valid(newACL) != 0) {
        acl_free(newACL);
        allIsWell = false;
    } else {
        acl_free(m_acl);
        m_acl = newACL;
    }
    return allIsWell;
}
#endif

bool KACL::setNamedUserPermissions(const QString &name, unsigned short permissions)
{
#if HAVE_POSIX_ACL
    return d->setNamedUserOrGroupPermissions(name, permissions, ACL_USER);
#else
    Q_UNUSED(name);
    Q_UNUSED(permissions);
    return true;
#endif
}

ACLUserPermissionsList KACL::allUserPermissions() const
{
    ACLUserPermissionsList list;
#if HAVE_POSIX_ACL
    acl_entry_t entry;
    int ret = acl_get_entry(d->m_acl, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag ==  ACL_USER) {
            const unique_ptr_acl_free idptr(acl_get_qualifier(entry), acl_free);
            const uid_t id = *(static_cast<uid_t *>(idptr.get()));
            QString name = d->getUserName(id);
            unsigned short permissions = entryToPermissions(entry);
            ACLUserPermissions pair = qMakePair(name, permissions);
            list.append(pair);
        }
        ret = acl_get_entry(d->m_acl, ACL_NEXT_ENTRY, &entry);
    }
#endif
    return list;
}

#if HAVE_POSIX_ACL
bool KACL::KACLPrivate::setAllUsersOrGroups(const QList< QPair<QString, unsigned short> > &list, acl_tag_t type)
{
    bool allIsWell = true;
    bool atLeastOneUserOrGroup = false;

    // make working copy, in case something goes wrong
    acl_t newACL = acl_dup(m_acl);
    acl_entry_t entry;

    // clear user entries
    int ret = acl_get_entry(newACL, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag ==  type) {
            acl_delete_entry(newACL, entry);
            // we have to start from the beginning, the iterator is
            // invalidated, on deletion
            ret = acl_get_entry(newACL, ACL_FIRST_ENTRY, &entry);
        } else {
            ret = acl_get_entry(newACL, ACL_NEXT_ENTRY, &entry);
        }
    }

    // now add the entries from the list
    QList< QPair<QString, unsigned short> >::const_iterator it = list.constBegin();
    while (it != list.constEnd()) {
        acl_create_entry(&newACL, &entry);
        acl_set_tag_type(entry, type);
        int id = type == ACL_USER ? getUidForName((*it).first) : getGidForName((*it).first);
        if (id == -1 || acl_set_qualifier(entry, &id) != 0) {
            // user or group doesn't exist => error
            acl_delete_entry(newACL, entry);
            allIsWell = false;
            break;
        } else {
            permissionsToEntry(entry, (*it).second);
            atLeastOneUserOrGroup = true;
        }
        ++it;
    }
    if (allIsWell && atLeastOneUserOrGroup) {
        // 23.1.1 of 1003.1e states that as soon as there is a named user or
        // named group entry, there needs to be a mask entry as well, so add
        // one, if the user hasn't explicitly set one.
        if (entryForTag(newACL, ACL_MASK) == nullptr) {
            acl_calc_mask(&newACL);
        }
    }
    if (allIsWell && (acl_valid(newACL) == 0)) {
        acl_free(m_acl);
        m_acl = newACL;
    } else {
        acl_free(newACL);
    }
    return allIsWell;
}
#endif

bool KACL::setAllUserPermissions(const ACLUserPermissionsList &users)
{
#if HAVE_POSIX_ACL
    return d->setAllUsersOrGroups(users, ACL_USER);
#else
    Q_UNUSED(users);
    return true;
#endif
}

/**************************
 * Deal with named groups  *
 **************************/

unsigned short KACL::namedGroupPermissions(const QString &name, bool *exists) const
{
    *exists = false;
#if HAVE_POSIX_ACL
    acl_entry_t entry;
    int ret = acl_get_entry(d->m_acl, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag ==  ACL_GROUP) {
            const unique_ptr_acl_free idptr(acl_get_qualifier(entry), acl_free);
            const gid_t id = *(static_cast<gid_t *>(idptr.get()));
            if (d->getGroupName(id) == name) {
                *exists = true;
                return entryToPermissions(entry);
            }
        }
        ret = acl_get_entry(d->m_acl, ACL_NEXT_ENTRY, &entry);
    }
#else
    Q_UNUSED(name);
#endif
    return 0;
}

bool KACL::setNamedGroupPermissions(const QString &name, unsigned short permissions)
{
#if HAVE_POSIX_ACL
    return d->setNamedUserOrGroupPermissions(name, permissions, ACL_GROUP);
#else
    Q_UNUSED(name);
    Q_UNUSED(permissions);
    return true;
#endif
}

ACLGroupPermissionsList KACL::allGroupPermissions() const
{
    ACLGroupPermissionsList list;
#if HAVE_POSIX_ACL
    acl_entry_t entry;
    int ret = acl_get_entry(d->m_acl, ACL_FIRST_ENTRY, &entry);
    while (ret == 1) {
        acl_tag_t currentTag;
        acl_get_tag_type(entry, &currentTag);
        if (currentTag ==  ACL_GROUP) {
            const unique_ptr_acl_free idptr(acl_get_qualifier(entry), acl_free);
            const gid_t id = *(static_cast<gid_t *>(idptr.get()));
            QString name = d->getGroupName(id);
            unsigned short permissions = entryToPermissions(entry);
            ACLGroupPermissions pair = qMakePair(name, permissions);
            list.append(pair);
        }
        ret = acl_get_entry(d->m_acl, ACL_NEXT_ENTRY, &entry);
    }
#endif
    return list;
}

bool KACL::setAllGroupPermissions(const ACLGroupPermissionsList &groups)
{
#if HAVE_POSIX_ACL
    return d->setAllUsersOrGroups(groups, ACL_GROUP);
#else
    Q_UNUSED(groups);
    return true;
#endif
}

/**************************
 * from and to string     *
 **************************/

bool KACL::setACL(const QString &aclStr)
{
    bool ret = false;
#if HAVE_POSIX_ACL
    acl_t temp = acl_from_text(aclStr.toLatin1().constData());
    if (acl_valid(temp) != 0) {
        // TODO errno is set, what to do with it here?
        acl_free(temp);
    } else {
        if (d->m_acl) {
            acl_free(d->m_acl);
        }
        d->m_acl = temp;
        ret = true;
    }
#else
    Q_UNUSED(aclStr);
#endif
    return ret;
}

QString KACL::asString() const
{
#if HAVE_POSIX_ACL
    ssize_t size = 0;
    char *txt = acl_to_text(d->m_acl, &size);
    const QString ret = QString::fromLatin1(txt, size);
    acl_free(txt);
    return ret;
#else
    return QString();
#endif
}

// helpers

#if HAVE_POSIX_ACL
QString KACL::KACLPrivate::getUserName(uid_t uid) const
{
    auto it = m_usercache.find(uid);
    if (it == m_usercache.end()) {
        struct passwd *user = getpwuid(uid);
        if (user) {
            it = m_usercache.insert(uid, QString::fromLatin1(user->pw_name));
        } else {
            return QString::number(uid);
        }
    }
    return *it;
}

QString KACL::KACLPrivate::getGroupName(gid_t gid) const
{
    auto it = m_groupcache.find(gid);
    if (it == m_groupcache.end()) {
        struct group *grp = getgrgid(gid);
        if (grp) {
            it = m_groupcache.insert(gid, QString::fromLatin1(grp->gr_name));
        } else {
            return QString::number(gid);
        }
    }
    return *it;
}
#endif

void KACL::virtual_hook(int, void *)
{
    /*BASE::virtual_hook( id, data );*/
}

QDataStream &operator<< (QDataStream &s, const KACL &a)
{
    s << a.asString();
    return s;
}

QDataStream &operator>> (QDataStream &s, KACL &a)
{
    QString str;
    s >> str;
    a.setACL(str);
    return s;
}

