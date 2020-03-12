/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "archive.h"

#include "api_priv.h"
#include "file.h"
#include "util/notify.h"

namespace ngs {

ArchiveFolder::ArchiveFolder(ObjectContainer * const parent,
                             const std::string &name,
                             const std::string &path) :
    Folder(parent, name, path)
{
    m_type = CAT_CONTAINER_ARCHIVE_DIR;
}

bool ArchiveFolder::canCreate(const enum ngsCatalogObjectType type) const
{
    ngsUnused(type);
    return false;
}

bool ArchiveFolder::canDestroy() const
{
    return false;
}

Archive::Archive(ObjectContainer * const parent,
                 const enum ngsCatalogObjectType type,
                 const std::string &name,
                 const std::string &path) :
    ArchiveFolder(parent, name, path)
{
    m_type = type;
    m_path = pathPrefix(type) + m_path;
}

bool Archive::canDestroy() const
{
    return Folder::canDestroy();
}

bool Archive::destroy()
{
    std::string sysPath = m_path.substr(Archive::pathPrefix(m_type).size());
    if(!File::deleteFile(sysPath)) {
        return false;
    }

    return ArchiveFolder::destroy();
}

std::string Archive::pathPrefix(const enum ngsCatalogObjectType type)
{
    switch (type) {
    case CAT_CONTAINER_ARCHIVE_ZIP:
        return "/vsizip/";
    default:
        return "";
    }
}


}
