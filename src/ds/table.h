/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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
#ifndef NGSTABLE_H
#define NGSTABLE_H

// gdal
#include "ogrsf_frmts.h"

#include "catalog/object.h"
#include "ngstore/codes.h"

#include "util/mutex.h"
#include "util/options.h"
#include "util/progress.h"

namespace ngs {

constexpr const char* LOG_EDIT_HISTORY_KEY = "LOG_EDIT_HISTORY";

typedef struct _Field {
    std::string m_name;
    std::string m_originalName;
    std::string m_alias;
    OGRFieldType m_type;
} Field;

/**
 * FieldMapPtr class
 */
class FieldMapPtr : public std::shared_ptr<int>
{
public:
    explicit FieldMapPtr(const std::vector<Field> &src,
                         const std::vector<Field> &dst);
    explicit FieldMapPtr(unsigned long size);
    int &operator[](int key);
    const int &operator[](int key) const;
    void matchFields(const std::vector<Field> &src,
                       const std::vector<Field> &dst);
};

class Table;

/**
 * FeaturePtr class
 */
class FeaturePtr : public std::shared_ptr<OGRFeature>
{
public:
    enum class DumpOutputType {
        HASH,
        HASH_FULL,
        HASH_STYLE,
        SIMPLE
    };

    /**
     * Attachment info struct
     */
    typedef struct _attachmentInfo {
        GIntBig id;
        std::string name;
        std::string description;
        std::string path;
        GIntBig size;
    } AttachmentInfo;
public:
    FeaturePtr(OGRFeature *feature, Table *table = nullptr);
    FeaturePtr(OGRFeature *feature, const Table *table);
    FeaturePtr();
    FeaturePtr &operator=(OGRFeature *feature);
    operator OGRFeature*() const;
    std::string dump(enum DumpOutputType type = DumpOutputType::HASH) const;
    GIntBig addAttachment(const std::string &fileName,
                          const std::string &description,
                          const std::string &filePath,
                          const Options &options = Options(),
                          bool logEdits = true);
    GIntBig addAttachment(const AttachmentInfo &info,
                          const Options &options = Options(),
                          bool logEdits = true);
    std::vector<FeaturePtr::AttachmentInfo> attachments() const;
    bool deleteAttachment(GIntBig aid, bool logEdits = true);
    bool deleteAttachments(bool logEdits = true);
    bool updateAttachment(GIntBig aid, const std::string &fileName,
                          const std::string &description, bool logEdits = true);

    Table *table() const;
    void setTable(Table *table);

protected:
    Table *m_table;
};

using TablePtr = std::shared_ptr<Table>;

/**
 * Table class
 */
class Table : public Object
{
    friend class FeaturePtr;
    friend class Dataset;
    friend class Folder;
public:
    explicit Table(OGRLayer *layer,
                   ObjectContainer * const parent = nullptr,
                   const enum ngsCatalogObjectType type = CAT_TABLE_ANY,
                   const std::string &name = "");
    virtual ~Table() override;
    virtual FeaturePtr createFeature() const;
    virtual FeaturePtr getFeature(GIntBig id) const;
    virtual bool insertFeature(const FeaturePtr &feature, bool logEdits = true);
    virtual bool updateFeature(const FeaturePtr &feature, bool logEdits = true);
    virtual bool deleteFeature(GIntBig id, bool logEdits = true);
    virtual bool deleteFeatures(bool logEdits = true);
    GIntBig featureCount(bool force = false) const;
    void reset() const;
    void setAttributeFilter(const std::string &filter = "");
    virtual FeaturePtr nextFeature() const;
    virtual int copyRows(const TablePtr srcTable,
                         const FieldMapPtr fieldMap,
                         const Progress &progress = Progress(),
                         const Options &options = Options());

    std::string fidColumn() const;
    const std::vector<Field> &fields() const;
    virtual GIntBig addAttachment(GIntBig fid, const std::string &fileName,
                                  const std::string &description,
                                  const std::string &filePath,
                                  const Options &options = Options(),
                                  bool logEdits = true);
    virtual bool deleteAttachment(GIntBig fid, GIntBig aid, bool logEdits = true);
    virtual bool deleteAttachments(GIntBig fid, bool logEdits = true);
    virtual bool updateAttachment(GIntBig fid, GIntBig aid,
                                  const std::string &fileName,
                                  const std::string &description,
                                  bool logEdits = true);
    virtual std::vector<FeaturePtr::AttachmentInfo> attachments(
            GIntBig fid) const;

    std::string getAttachmentPath(GIntBig fid, GIntBig aid,
                                  bool createPath = false) const;

    OGRFeatureDefn *definition() const;
    OGRLayer *attachmentsTable(bool init = false) const;

    // Edit log
    virtual void deleteEditOperation(const ngsEditOperation &op);
    virtual std::vector<ngsEditOperation> editOperations();

    virtual bool sync() override;

    // Object interface
public:
    virtual bool canDestroy() const override;
    virtual bool destroy() override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool setProperty(const std::string &name, const std::string &value,
                             const std::string &domain) override;
    virtual void deleteProperties(const std::string &domain) override;

    // static
public:
    static OGRFieldType fieldTypeFromName(const std::string &name);

protected:
    bool initAttachmentsTable() const;
    bool initEditHistoryTable() const;
    std::string getAttachmentsPath(bool create = false) const;
    bool saveEditHistory();

    virtual void fillFields() const;
    virtual void logEditOperation(const FeaturePtr &opFeature);
    virtual FeaturePtr logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code);
    virtual bool checkSetProperty(const std::string &key, const std::string &value,
                                  const std::string &domain);
    virtual std::string fullPropertyDomain(const std::string &domain) const;
    virtual std::string storeName() const;
    // Events
    virtual void onFeatureInserted(FeaturePtr feature);
    virtual void onFeatureUpdated(FeaturePtr oldFeature, FeaturePtr newFeature);
    virtual void onFeatureDeleted(FeaturePtr delFeature);
    virtual void onFeaturesDeleted();
    virtual void onRowCopied(FeaturePtr srcFeature, FeaturePtr dstFature,
                             const Options &options = Options());
    virtual bool onRowsCopied(const TablePtr srcTable, const Progress &progress,
                              const Options &options = Options());

protected:
    mutable OGRLayer *m_layer;
    mutable OGRLayer *m_attTable;
    mutable OGRLayer *m_editHistoryTable;
    mutable std::vector<Field> m_fields;
    Mutex m_featureMutex;
};

}
#endif // NGSTABLE_H
