/*
 *  KUser - represent a user/account
 *  Copyright (C) 2002 Tim Jansen <tim@tjansen.de>
 *  Copyright (C) 2014 Alex Richardson <arichardson.kde@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include <kuser.h>

#include <QtCore/QMutableStringListIterator>
#include <QtCore/QDir>

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>
#include <errno.h>

class KUser::Private : public QSharedData
{
public:
    uid_t uid;
    gid_t gid;
    QString loginName;
    QString homeDir, shell;
    QMap<UserProperty, QVariant> properties;

    Private() : uid(uid_t(-1)), gid(gid_t(-1)) {}
    Private(const char *name) : uid(uid_t(-1)), gid(gid_t(-1))
    {
        fillPasswd(name ? ::getpwnam(name) : 0);
    }
    Private(const passwd *p) : uid(uid_t(-1)), gid(gid_t(-1))
    {
        fillPasswd(p);
    }

    void fillPasswd(const passwd *p)
    {
        if (p) {
            QString gecos = QString::fromLocal8Bit(p->pw_gecos);
            QStringList gecosList = gecos.split(QLatin1Char(','));
            // fill up the list, should be at least 4 entries
            while (gecosList.size() < 4) {
                gecosList << QString();
            }

            uid = p->pw_uid;
            gid = p->pw_gid;
            loginName = QString::fromLocal8Bit(p->pw_name);
            properties[KUser::FullName] = QVariant(gecosList[0]);
            properties[KUser::RoomNumber] = QVariant(gecosList[1]);
            properties[KUser::WorkPhone] = QVariant(gecosList[2]);
            properties[KUser::HomePhone] = QVariant(gecosList[3]);
            if (uid == ::getuid() && uid == ::geteuid()) {
                homeDir = QFile::decodeName(qgetenv("HOME"));
            }
            if (homeDir.isEmpty()) {
                homeDir = QFile::decodeName(p->pw_dir);
            }
            shell = QString::fromLocal8Bit(p->pw_shell);
        }
    }
};

KUser::KUser(UIDMode mode)
{
    uid_t _uid = ::getuid(), _euid;
    if (mode == UseEffectiveUID && (_euid = ::geteuid()) != _uid) {
        d = new Private(::getpwuid(_euid));
    } else {
        d = new Private(qgetenv("LOGNAME").constData());
        if (d->uid != _uid) {
            d = new Private(qgetenv("USER").constData());
            if (d->uid != _uid) {
                d = new Private(::getpwuid(_uid));
            }
        }
    }
}

KUser::KUser(K_UID _uid)
    : d(new Private(::getpwuid(_uid)))
{
}

KUser::KUser(KUserId _uid)
: d(new Private(::getpwuid(_uid.nativeId())))
{
}

KUser::KUser(const QString &name)
    : d(new Private(name.toLocal8Bit().data()))
{
}

KUser::KUser(const char *name)
    : d(new Private(name))
{
}

KUser::KUser(const passwd *p)
    : d(new Private(p))
{
}

KUser::KUser(const KUser &user)
    : d(user.d)
{
}

KUser &KUser::operator =(const KUser &user)
{
    d = user.d;
    return *this;
}

bool KUser::operator ==(const KUser &user) const
{
    return isValid() && (d->uid == user.d->uid);
}

bool KUser::operator !=(const KUser &user) const
{
    return !operator==(user);
}

bool KUser::isValid() const
{
    return d->uid != uid_t(-1);
}

KUserId KUser::userId() const
{
    return KUserId(d->uid);
}

KGroupId KUser::groupId() const
{
    return KGroupId(d->gid);
}

bool KUser::isSuperUser() const
{
    return d->uid == 0;
}

QString KUser::loginName() const
{
    return d->loginName;
}

QString KUser::homeDir() const
{
    return d->homeDir;
}

QString KUser::faceIconPath() const
{
    QString pathToFaceIcon(homeDir() + QDir::separator() + QLatin1String(".face.icon"));

    if (QFile::exists(pathToFaceIcon)) {
        return pathToFaceIcon;
    }

    return QString();
}

QString KUser::shell() const
{
    return d->shell;
}

QList<KUserGroup> KUser::groups(uint maxCount) const
{
    QList<KUserGroup> result;
    const QList<KUserGroup> allGroups = KUserGroup::allGroups();
    QList<KUserGroup>::const_iterator it;
    for (it = allGroups.begin(); it != allGroups.end(); ++it) {
        const QList<KUser> users = (*it).users();
        if (users.contains(*this)) {
            if ((uint)result.size() >= maxCount) {
                break;
            }
            result.append(*it);
        }
    }
    return result;
}

QStringList KUser::groupNames(uint maxCount) const
{
    QStringList result;
    const QList<KUserGroup> allGroups = KUserGroup::allGroups();
    QList<KUserGroup>::const_iterator it;
    for (it = allGroups.begin(); it != allGroups.end(); ++it) {
        const QList<KUser> users = (*it).users();
        if (users.contains(*this)) {
            if ((uint)result.size() >= maxCount) {
                break;
            }
            result.append((*it).name());
        }
    }
    return result;
}

QVariant KUser::property(UserProperty which) const
{
    return d->properties.value(which);
}

QList<KUser> KUser::allUsers(uint maxCount)
{
    QList<KUser> result;

    passwd *p;
    setpwent();

    for (uint i = 0; i < maxCount && (p = getpwent()); ++i) {
        result.append(KUser(p));
    }

    endpwent();

    return result;
}

QStringList KUser::allUserNames(uint maxCount)
{
    QStringList result;

    passwd *p;
    setpwent();

    for (uint i = 0; i < maxCount && (p = getpwent()); ++i) {
        result.append(QString::fromLocal8Bit(p->pw_name));
    }

    endpwent();
    return result;
}

KUser::~KUser()
{
}

class KUserGroup::Private : public QSharedData
{
public:
    gid_t gid;
    QString name;
    QList<KUser> users;

    Private() : gid(gid_t(-1)) {}
    Private(const char *_name) : gid(gid_t(-1))
    {
        fillGroup(_name ? ::getgrnam(_name) : 0);
    }
    Private(const ::group *p) : gid(gid_t(-1))
    {
        fillGroup(p);
    }

    void fillGroup(const ::group *p)
    {
        if (p) {
            gid = p->gr_gid;
            name = QString::fromLocal8Bit(p->gr_name);
            for (char **user = p->gr_mem; *user; user++) {
                users.append(KUser(*user));
            }
        }
    }
};

KUserGroup::KUserGroup(KUser::UIDMode mode)
{
    d = new Private(getgrgid(KUser(mode).groupId().nativeId()));
}

KUserGroup::KUserGroup(K_GID _gid)
    : d(new Private(getgrgid(_gid)))
{
}

KUserGroup::KUserGroup(KGroupId _gid)
    : d(new Private(getgrgid(_gid.nativeId())))
{
}

KUserGroup::KUserGroup(const QString &_name)
    : d(new Private(_name.toLocal8Bit().data()))
{
}

KUserGroup::KUserGroup(const char *_name)
    : d(new Private(_name))
{
}

KUserGroup::KUserGroup(const ::group *g)
    : d(new Private(g))
{
}

KUserGroup::KUserGroup(const KUserGroup &group)
    : d(group.d)
{
}

KUserGroup &KUserGroup::operator =(const KUserGroup &group)
{
    d = group.d;
    return *this;
}

bool KUserGroup::operator ==(const KUserGroup &group) const
{
    return isValid() && (d->gid == group.d->gid);
}

bool KUserGroup::operator !=(const KUserGroup &user) const
{
    return !operator==(user);
}

bool KUserGroup::isValid() const
{
    return d->gid != gid_t(-1);
}

KGroupId KUserGroup::groupId() const
{
    return KGroupId(d->gid);
}

QString KUserGroup::name() const
{
    return d->name;
}

QList<KUser> KUserGroup::users(uint maxCount) const
{
    if ((int)maxCount < 0) {
        return d->users;
    }
    return d->users.mid(0, (int)maxCount);
}

QStringList KUserGroup::userNames(uint maxCount) const
{
    QStringList result;
    for (uint i = 0; i < (uint)d->users.size() && i < maxCount; ++i) {
        result.append(d->users[i].loginName());
    }
    return result;
}

QList<KUserGroup> KUserGroup::allGroups(uint maxCount)
{
    QList<KUserGroup> result;

    ::group *g;
    setgrent();

    for (uint i = 0; i < maxCount && (g = getgrent()); ++i) {
        result.append(KUserGroup(g));
    }

    endgrent();

    return result;
}

QStringList KUserGroup::allGroupNames(uint maxCount)
{
    QStringList result;

    ::group *g;
    setgrent();

    for (uint i = 0; i < maxCount && (g = getgrent()); ++i) {
        result.append(QString::fromLocal8Bit(g->gr_name));
    }

    endgrent();

    return result;
}

KUserGroup::~KUserGroup()
{
}

KUserId KUserId::fromName(const QString& name)
{
    QByteArray name8Bit = name.toLocal8Bit();
    struct passwd* p = ::getpwnam(name8Bit.constData());
    if (!p) {
        qWarning("Failed to lookup user %s: %s", name8Bit.constData(), strerror(errno));
        return KUserId();
    }
    return KUserId(p->pw_uid);
}

KGroupId KGroupId::fromName(const QString& name)
{
    QByteArray name8Bit = name.toLocal8Bit();
    struct group* g = ::getgrnam(name8Bit.constData());
    if (!g) {
        qWarning("Failed to lookup group %s: %s", name8Bit.constData(), strerror(errno));
        return KGroupId();
    }
    return KGroupId(g->gr_gid);
}

KUserId KUserId::currentUserId() { return KUserId(getuid()); }

KUserId KUserId::currentEffectiveUserId() { return KUserId(geteuid()); }

KGroupId KGroupId::currentGroupId() { return KGroupId(getgid()); }

KGroupId KGroupId::currentEffectiveGroupId() { return KGroupId(getegid()); }
