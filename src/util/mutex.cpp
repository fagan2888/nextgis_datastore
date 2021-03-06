/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2018 NextGIS, <info@nextgis.com>
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
#include "mutex.h"

namespace ngs {

Mutex::Mutex() : m_mutex(CPLCreateMutex())
{
    CPLReleaseMutex(m_mutex);
}

Mutex::~Mutex()
{
    CPLDestroyMutex(m_mutex);
}

void Mutex::acquire(double timeout)
{
    CPLAcquireMutex(m_mutex, timeout);
}

void Mutex::release()
{
    CPLReleaseMutex(m_mutex);
}

MutexHolder::MutexHolder(const Mutex &mutex, double timeout) :
    m_mutex(const_cast<Mutex &>(mutex))
{
    m_mutex.acquire(timeout);
}

MutexHolder::~MutexHolder()
{
    m_mutex.release();
}

}
