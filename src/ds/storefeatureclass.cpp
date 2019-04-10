/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include <ctime>
#include "storefeatureclass.h"

#include "datastore.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "catalog/ngw.h"
#include "ngstore/version.h"
#include "util/error.h"

#ifdef WIN32
#define timegm _mkgmtime
#endif

namespace ngs {

//------------------------------------------------------------------------------
// StoreObject
//------------------------------------------------------------------------------

StoreObject::StoreObject(OGRLayer *layer) : m_storeIntLayer(layer)
{
}

FeaturePtr StoreObject::getFeatureByRemoteId(GIntBig rid) const
{
    const Table *table = dynamic_cast<const Table*>(this);
    if(nullptr == table) {
        return FeaturePtr();
    }

    Dataset *dataset = dynamic_cast<Dataset*>(table->parent());
    DatasetExecuteSQLLockHolder holder(dataset);
    m_storeIntLayer->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB,
                                                   REMOTE_ID_KEY, rid));
    FeaturePtr out(m_storeIntLayer->GetNextFeature(), table);
    m_storeIntLayer->SetAttributeFilter(nullptr);
    return out;
}

bool StoreObject::setFeatureAttachmentRemoteId(GIntBig aid, GIntBig rid)
{
    Table *table = dynamic_cast<Table*>(this);
    if(nullptr == table) {
        return false;
    }

    if(!table->initAttachmentsTable()) {
        return false;
    }

    OGRLayer *attTable = table->m_attTable;
    if(nullptr == attTable) {
        return false;
    }

    Dataset *dataset = dynamic_cast<Dataset*>(table->parent());
    DatasetExecuteSQLLockHolder holder(dataset);
    FeaturePtr attFeature = attTable->GetFeature(aid);
    if(!attFeature) {
        return false;
    }

    attFeature->SetField(REMOTE_ID_KEY, rid);
    return attTable->SetFeature(attFeature) == OGRERR_NONE;
}


void StoreObject::setRemoteId(FeaturePtr feature, GIntBig rid)
{
    feature->SetField(REMOTE_ID_KEY, rid);
}

GIntBig StoreObject::getRemoteId(FeaturePtr feature)
{
    if(feature)
        return feature->GetFieldAsInteger64(REMOTE_ID_KEY);
    return NOT_FOUND;
}

std::vector<ngsEditOperation> StoreObject::fillEditOperations(
        OGRLayer *editHistoryTable) const
{
    std::vector<ngsEditOperation> out;
    if(nullptr == editHistoryTable) {
        return out;
    }
    FeaturePtr feature;
    editHistoryTable->ResetReading();
    while((feature = editHistoryTable->GetNextFeature())) {
        ngsEditOperation op;
        op.fid = feature->GetFieldAsInteger64(FEATURE_ID_FIELD);
        op.aid = feature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
        op.code = static_cast<enum ngsChangeCode>(feature->GetFieldAsInteger64(
                                                      OPERATION_FIELD));
        op.rid = StoreObject::getRemoteId(feature);
        op.arid = feature->GetFieldAsInteger64(ATTACHMENT_REMOTE_ID_KEY);
        if(op.arid == NOT_FOUND) {
            op.arid = getAttachmentRemoteId(op.aid);
        }
        out.push_back(op);
    }

    return out;
}

GIntBig StoreObject::getAttachmentRemoteId(GIntBig aid) const
{
    const Table *table = dynamic_cast<const Table*>(this);
    if(nullptr == table) {
        return NOT_FOUND;
    }

    OGRLayer *attTable = table->m_attTable;
    if(attTable == nullptr) {
        return NOT_FOUND;
    }

    FeaturePtr attFeature = attTable->GetFeature(aid);
    GIntBig rid = NOT_FOUND;
    if(attFeature) {
        rid = getRemoteId(attFeature);
    }
    return rid;
}

//------------------------------------------------------------------------------
// StoreTable
//------------------------------------------------------------------------------
StoreTable::StoreTable(OGRLayer *layer, ObjectContainer * const parent,
                       const std::string &name) :
    Table(layer, parent, CAT_TABLE_GPKG, name),
    StoreObject(layer)
{
}

void StoreTable::fillFields() const
{
    Table::fillFields();
    // Hide remote id field from user
    if(compare(m_fields.back().m_name, REMOTE_ID_KEY)) {
        m_fields.pop_back();
    }
}

std::vector<Table::AttachmentInfo> StoreTable::attachments(GIntBig fid) const
{
    std::vector<AttachmentInfo> out;

    if(!initAttachmentsTable()) {
        return out;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));

    m_attTable->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB,
                                              ATTACH_FEATURE_ID_FIELD, fid));
    //m_attTable->ResetReading();
    FeaturePtr attFeature;
    while((attFeature = m_attTable->GetNextFeature())) {
        AttachmentInfo info;
        info.name = attFeature->GetFieldAsString(ATTACH_FILE_NAME_FIELD);
        info.description = attFeature->GetFieldAsString(ATTACH_DESCRIPTION_FIELD);
        info.id = attFeature->GetFID();
        info.rid = attFeature->GetFieldAsInteger64(REMOTE_ID_KEY);

        std::string attFeaturePath = File::formFileName(getAttachmentsPath(),
                                                        std::to_string(fid), "");
        info.path = File::formFileName(attFeaturePath, std::to_string(info.id),
                                       "");
        info.size = File::fileSize(info.path);

        out.push_back(info);
    }

    return out;
}

GIntBig StoreTable::addAttachment(GIntBig fid, const std::string &fileName,
                                  const std::string &description,
                                  const std::string &filePath,
                                  const Options &options, bool logEdits)
{
    if(!initAttachmentsTable()) {
        return NOT_FOUND;
    }
    bool move = options.asBool("MOVE", false);
    GIntBig rid = options.asLong("RID", INIT_RID_COUNTER);

    FeaturePtr newAttachment = OGRFeature::CreateFeature(
                m_attTable->GetLayerDefn());

    newAttachment->SetField(ATTACH_FEATURE_ID_FIELD, fid);
    newAttachment->SetField(ATTACH_FILE_NAME_FIELD, fileName.c_str());
    newAttachment->SetField(ATTACH_DESCRIPTION_FIELD, description.c_str());
    newAttachment->SetField(REMOTE_ID_KEY, rid);

    if(m_attTable->CreateFeature(newAttachment) == OGRERR_NONE) {
        std::string dstTablePath = getAttachmentsPath();
        if(!Folder::isExists(dstTablePath)) {
            Folder::mkDir(dstTablePath);
        }
        std::string dstFeaturePath = File::formFileName(dstTablePath,
                                                        std::to_string(fid), "");
        if(!Folder::isExists(dstFeaturePath)) {
            Folder::mkDir(dstFeaturePath);
        }

        std::string dstPath = File::formFileName(dstFeaturePath,
                                                 std::to_string(newAttachment->GetFID()),
                                                 "");
        if(Folder::isExists(filePath)) {
            if(move) {
                File::moveFile(filePath, dstPath);
            }
            else {
                File::copyFile(filePath, dstPath);
            }
        }
        if(logEdits) {
            FeaturePtr feature = m_layer->GetFeature(fid);
            FeaturePtr logFeature = logEditFeature(feature, newAttachment,
                                                   CC_CREATE_ATTACHMENT);
            logEditOperation(logFeature);
        }
        return newAttachment->GetFID();
    }

    return NOT_FOUND;
}

bool StoreTable::setProperty(const std::string &key, const std::string &value,
                                    const std::string &domain)
{
    checkSetProperty(key, value, domain);
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_layer->SetMetadataItem(key.c_str(),
                                    value.c_str(), domain.c_str()) == OGRERR_NONE;
}

std::string StoreTable::property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const
{
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    const char *item = m_layer->GetMetadataItem(key.c_str(), domain.c_str());
    return item != nullptr ? item : defaultValue;
}

Properties StoreTable::properties(
        const std::string &domain) const
{
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return Properties(m_layer->GetMetadata(domain.c_str()));
}

void StoreTable::deleteProperties(const std::string &domain)
{
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    m_layer->SetMetadata(nullptr, domain.c_str());
}

std::vector<ngsEditOperation> StoreTable::editOperations() const
{
    if(nullptr == m_editHistoryTable) {
        initEditHistoryTable();
    }

    if(nullptr == m_editHistoryTable) {
        return std::vector<ngsEditOperation>();
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return fillEditOperations(m_editHistoryTable);
}

FeaturePtr StoreTable::logEditFeature(FeaturePtr feature,
                                      FeaturePtr attachFeature, ngsChangeCode code)
{
    FeaturePtr logFeature = Table::logEditFeature(feature, attachFeature, code);
    if(logFeature) {
        logFeature->SetField(REMOTE_ID_KEY, StoreObject::getRemoteId(feature));
        logFeature->SetField(ATTACHMENT_REMOTE_ID_KEY,
                             StoreObject::getRemoteId(attachFeature));
    }
    return logFeature;
}

//------------------------------------------------------------------------------
// StoreFeatureClass
//------------------------------------------------------------------------------

StoreFeatureClass::StoreFeatureClass(OGRLayer *layer,
                                     ObjectContainer * const parent,
                                     const std::string &name) :
    FeatureClass(layer, parent, CAT_FC_GPKG, name),
    StoreObject(layer)
{
    if(m_zoomLevels.empty()) {
        fillZoomLevels();
    }
}

void StoreFeatureClass::fillFields() const
{
    Table::fillFields();
    // Hide remote id field from user
    if(compare(m_fields.back().m_name, REMOTE_ID_KEY)) {
        m_fields.pop_back();
    }
}

std::vector<Table::AttachmentInfo> StoreFeatureClass::attachments(GIntBig fid) const
{
    std::vector<AttachmentInfo> out;

    if(!initAttachmentsTable()) {
        return out;
    }

    Dataset *dataset = dynamic_cast<Dataset*>(m_parent);
    DatasetExecuteSQLLockHolder holder(dataset);

    m_attTable->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB,
                                              ATTACH_FEATURE_ID_FIELD, fid));
    //m_attTable->ResetReading();
    FeaturePtr attFeature;
    while((attFeature = m_attTable->GetNextFeature())) {
        AttachmentInfo info;
        info.name = attFeature->GetFieldAsString(ATTACH_FILE_NAME_FIELD);
        info.description = attFeature->GetFieldAsString(ATTACH_DESCRIPTION_FIELD);
        info.id = attFeature->GetFID();
        info.rid = attFeature->GetFieldAsInteger64(REMOTE_ID_KEY);

        std::string attFeaturePath = File::formFileName(getAttachmentsPath(),
                                                        std::to_string(fid), "");
        info.path = File::formFileName(attFeaturePath, std::to_string(info.id),
                                       "");
        info.size = File::fileSize(info.path);

        out.push_back(info);
    }

    return out;
}

GIntBig StoreFeatureClass::addAttachment(GIntBig fid, const std::string &fileName,
                                         const std::string &description,
                                         const std::string &filePath,
                                         const Options &options, bool logEdits)
{
    if(!initAttachmentsTable()) {
        return NOT_FOUND;
    }
    bool move = options.asBool("MOVE", false);
    GIntBig rid = options.asLong("RID", INIT_RID_COUNTER);

    FeaturePtr newAttachment = OGRFeature::CreateFeature(
                m_attTable->GetLayerDefn());

    newAttachment->SetField(ATTACH_FEATURE_ID_FIELD, fid);
    newAttachment->SetField(ATTACH_FILE_NAME_FIELD, fileName.c_str());
    newAttachment->SetField(ATTACH_DESCRIPTION_FIELD, description.c_str());
    newAttachment->SetField(REMOTE_ID_KEY, rid);

    if(m_attTable->CreateFeature(newAttachment) == OGRERR_NONE) {
        std::string dstTablePath = getAttachmentsPath();
        if(!Folder::isExists(dstTablePath)) {
            Folder::mkDir(dstTablePath);
        }
        std::string dstFeaturePath = File::formFileName(dstTablePath,
                                                        std::to_string(fid), "");
        if(!Folder::isExists(dstFeaturePath)) {
            Folder::mkDir(dstFeaturePath);
        }

        std::string dstPath = File::formFileName(dstFeaturePath,
                                                 std::to_string(newAttachment->GetFID()),
                                                 "");
        if(Folder::isExists(filePath)) {
            if(move) {
                File::moveFile(filePath, dstPath);
            }
            else {
                File::copyFile(filePath, dstPath);
            }
        }

        if(logEdits) {
            FeaturePtr feature = m_layer->GetFeature(fid);
            FeaturePtr logFeature = logEditFeature(feature, newAttachment,
                                                   CC_CREATE_ATTACHMENT);
            logEditOperation(logFeature);
        }
        return newAttachment->GetFID();
    }

    return NOT_FOUND;
}

bool StoreFeatureClass::setProperty(const std::string &key, const std::string &value,
                                    const std::string &domain)
{
    checkSetProperty(key, value, domain);
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_layer->SetMetadataItem(key.c_str(),
                                    value.c_str(), domain.c_str()) == OGRERR_NONE;
}

std::string StoreFeatureClass::property(const std::string &key, const std::string &defaultValue,
                                         const std::string &domain) const
{
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    const char *item = m_layer->GetMetadataItem(key.c_str(), domain.c_str());
    return item != nullptr ? item : defaultValue;
}

Properties StoreFeatureClass::properties(const std::string &domain) const
{
    return Properties(m_layer->GetMetadata(domain.c_str()));
}

void StoreFeatureClass::deleteProperties(const std::string &domain)
{
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    m_layer->SetMetadata(nullptr, domain.c_str());
}

std::vector<ngsEditOperation> StoreFeatureClass::editOperations() const
{
    if(nullptr == m_editHistoryTable) {
        initEditHistoryTable();
    }

    if(nullptr == m_editHistoryTable) {
        return std::vector<ngsEditOperation>();
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return fillEditOperations(m_editHistoryTable);
}

FeaturePtr StoreFeatureClass::logEditFeature(FeaturePtr feature,
                                             FeaturePtr attachFeature,
                                             ngsChangeCode code)
{
    FeaturePtr logFeature = Table::logEditFeature(feature, attachFeature, code);
    if(logFeature) {
        logFeature->SetField(REMOTE_ID_KEY, StoreObject::getRemoteId(feature));
        logFeature->SetField(ATTACHMENT_REMOTE_ID_KEY,
                             StoreObject::getRemoteId(attachFeature));
    }
    return logFeature;
}

//------------------------------------------------------------------------------
// TracksTable
//------------------------------------------------------------------------------
TracksTable::TracksTable(OGRLayer *layer, ObjectContainer * const parent) :
    FeatureClass(layer, parent, CAT_FC_GPKG, "Tracks"),
    m_lastTrackId(0),
    m_lastSegmentId(0),
    m_lastSegmentPtId(0)
{
    Dataset *dataset = dynamic_cast<Dataset*>(m_parent);
    TablePtr result = dataset->executeSQL("SELECT max(track_fid) FROM nga_tracks", "SQLITE");
    if(result) {
        FeaturePtr resultFeature = result->nextFeature();
        m_lastTrackId = resultFeature->GetFieldAsInteger(0);
    }
}

static long dateFieldToLong(const FeaturePtr &feature, int field, bool isString = true)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if(isString) {
        sscanf(feature->GetFieldAsString(field), "%d-%d-%dT%d:%d:%dZ",
              &year, &month, &day, &hour, &minute, &second);
    }
    else {
        feature->GetFieldAsDateTime(field, &year, &month, &day, &hour, &minute,
                                    &second, nullptr);
    }
    struct tm timeInfo = {second, minute, hour, day, month - 1, year - 1900};
    return timegm(&timeInfo);
}

void TracksTable::sync(int maxPointCount)
{
    MutexHolder holder(m_syncMutex); // Don't allow simultaneous syncing
    setAttributeFilter("synced = 0");
    OGRCoordinateTransformation *ct =
            OGRCreateCoordinateTransformation(
                m_spatialReference, OGRSpatialReference::GetWGS84SRS());
    OGRFeatureDefn *def = definition();
    int timeIndex = def->GetFieldIndex("time");
    int eleIndex = def->GetFieldIndex("ele");
    int satIndex = def->GetFieldIndex("sat");
    int fixIndex = def->GetFieldIndex("fix");
    int speedIndex = def->GetFieldIndex("speed");
    int accIndex = def->GetFieldIndex("pdop");

    std::string fid = m_layer->GetFIDColumn();

    CPLJSONArray *payload = new CPLJSONArray;
    FeaturePtr feature;
    std::vector<std::string> updateWhere;
    GIntBig first = std::numeric_limits<GIntBig>::max();
    GIntBig last = 0;

    while((feature = nextFeature())) {
        OGRGeometry *geom = feature->GetGeometryRef();
        OGRPoint *pt = static_cast<OGRPoint*>(geom);
        if(pt && pt->transform(ct) == OGRERR_NONE) {
            if(first > feature->GetFID()) {
                first = feature->GetFID();
            }
            if(last < feature->GetFID()) {
                last = feature->GetFID();
            }
            CPLJSONObject item;
            item.Add("lt", pt->getY());
            item.Add("ln", pt->getX());
            GInt64 timestamp = dateFieldToLong(feature, timeIndex, false);
            item.Add("ts", timestamp);
            item.Add("a", feature->GetFieldAsDouble(eleIndex));
            item.Add("s", feature->GetFieldAsInteger(satIndex));
            int fix = compare(feature->GetFieldAsString(fixIndex), "3d", true) ? 3 : 2;
            item.Add("ft", fix);
            item.Add("sp", feature->GetFieldAsDouble(speedIndex) * 3.6); // Convert from meters pre second to kilometers per hour
            item.Add("ha", feature->GetFieldAsDouble(accIndex));

            payload->Add(item);

            if(payload->Size() >= maxPointCount) {
                if(ngw::sendTrackPoints(payload->Format(CPLJSONObject::Plain))) {
                    updateWhere.emplace_back(
                        fid + " >= " + std::to_string(first) + " AND " +
                        fid + " <= " + std::to_string(last));
                }
                delete payload;
                payload = new CPLJSONArray;

                first = std::numeric_limits<GIntBig>::max();
                last = 0;
            }
        }
    }
    setAttributeFilter("");
    OGRCoordinateTransformation::DestroyCT(ct);

    if(payload->Size() > 0) {
        if(ngw::sendTrackPoints(payload->Format(CPLJSONObject::Plain))) {
            updateWhere.emplace_back(
                fid + " >= " + std::to_string(first) + " AND " +
                fid + " <= " + std::to_string(last));
        }
    }
    delete payload;

    if(!updateWhere.empty()) {
        Dataset *dataset = dynamic_cast<Dataset*>(m_parent);
        for(const auto &where : updateWhere) {
            dataset->executeSQL("UPDATE nga_tracks SET synced = 1 WHERE " + where,
                                "SQLITE");
        }
    }
}

std::vector<TrackInfo> TracksTable::getTracks() const
{
    std::vector<TrackInfo> out;
    Dataset *dataset = dynamic_cast<Dataset*>(m_parent);
    TablePtr result = dataset->executeSQL(
            "SELECT track_name, min(time_stamp), max(time_stamp) FROM nga_tracks GROUP BY track_fid",
            "SQLITE");
    FeaturePtr feature;
    while((feature = result->nextFeature())) {
        TrackInfo info = {feature->GetFieldAsString(0),
                          dateFieldToLong(feature, 1),
                          dateFieldToLong(feature, 2)};
        out.emplace_back(info);
    }
    return out;
}

bool TracksTable::addPoint(const std::string &name, double x, double y, double z, float accuracy, float speed,
        float course, long timeStamp, int satCount, bool newTrack, bool newSegment)
{
    FeaturePtr feature = createFeature();
    if(newTrack) {
        feature->SetField("track_fid", ++m_lastTrackId);
        m_lastSegmentId = 0;
        m_lastSegmentPtId = 0;
    }
    else {
        feature->SetField("track_fid", m_lastTrackId);
    }

    if(newSegment) {
        feature->SetField("track_seg_id", ++m_lastSegmentId);
        m_lastSegmentPtId = 0;
    }
    else {
        feature->SetField("track_seg_id", m_lastSegmentId);
    }

    feature->SetField("track_seg_point_id", ++m_lastSegmentPtId);

    feature->SetField("track_name", name.c_str());
    long gmtTimeStamp = timeStamp / 1000;
    std::tm *gmtTime = std::gmtime(&gmtTimeStamp);
    feature->SetField("time", gmtTime->tm_year + 1900, gmtTime->tm_mon + 1, gmtTime->tm_mday, gmtTime->tm_hour,
            gmtTime->tm_min, gmtTime->tm_sec);
    feature->SetField("sat", satCount);
    feature->SetField("speed", static_cast<double>(speed));
    feature->SetField("course", static_cast<double>(course));
    feature->SetField("pdop", static_cast<double>(accuracy));
    feature->SetField("fix", satCount > 3 ? "3d" : "2d");
    feature->SetField("ele", z);
    feature->SetField("desc", NGS_USERAGENT);

    OGRPoint *newPt = new OGRPoint(x, y);
    newPt->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());
    newPt->transformTo(m_spatialReference);
    feature->SetGeometryDirectly(newPt);

    resetError();
    // Lock all Dataset SQL queries here
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_layer->CreateFeature(feature) == OGRERR_NONE;
}

static std::string longToISO(long timeStamp)
{
    long gmtTimeStamp = timeStamp / 1000;
    std::tm *gmtTime = std::gmtime(&gmtTimeStamp);
    return CPLSPrintf("%d-%d-%dT%d:%d:%dZ", gmtTime->tm_year + 1900, gmtTime->tm_mon + 1, gmtTime->tm_mday, gmtTime->tm_hour,
                      gmtTime->tm_min, gmtTime->tm_sec);
}

void TracksTable::deletePoints(long start, long end)
{
    resetError();
    Dataset *dataset = dynamic_cast<Dataset*>(m_parent);
    dataset->executeSQL("DELETE FROM nga_tracks WHERE time_stamp >= '" + longToISO(start) +
    "' AND time_stamp <= '" + longToISO(end) + "'", "SQLITE");
}

ObjectPtr TracksTable::pointer() const
{
    DataStore *dataset = dynamic_cast<DataStore*>(m_parent);
    if(dataset == nullptr) {
        return ObjectPtr();
    }

    return dataset->getTracksTable();
}

} // namespace ngs

