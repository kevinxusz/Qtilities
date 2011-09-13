/****************************************************************************
**
** Copyright (c) 2009-2011, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "ObjectManager.h"
#include "QtilitiesProperty.h"
#include "QtilitiesCoreConstants.h"
#include "Observer.h"
#include "ObserverHints.h"
#include "SubjectTypeFilter.h"
#include "AbstractSubjectFilter.h"
#include "Factory.h"
#include "ActivityPolicyFilter.h"
#include "SubjectTypeFilter.h"
#include "ObserverRelationalTable.h"

#include <Logger.h>

#include <QVariant>
#include <QMap>
#include <QPointer>
#include <QtCore>
#include <QDomDocument>

using namespace Qtilities::Core::Constants;
using namespace Qtilities::Core::Properties;
using namespace Qtilities::Core::Interfaces;

struct Qtilities::Core::ObjectManagerPrivateData {
    ObjectManagerPrivateData() : object_pool(qti_def_GLOBAL_OBJECT_POOL,QObject::tr("Pool of exposed global objects.")),
    id(1)  { }

    QMap<int,QPointer<Observer> >               context_map;
    QMap<QString, IFactoryProvider*>            factory_map;
    QMap<QString, QList<QPointer<QObject> > >   meta_type_map;
    Observer                                    object_pool;
    int                                         id;
    Factory<QObject>                            qtilities_factory;
};

Qtilities::Core::ObjectManager::ObjectManager(QObject* parent) : IObjectManager(parent)
{
    d = new ObjectManagerPrivateData;
    d->object_pool.startProcessingCycle();
    setObjectName("Object Manager");

    // Add the standard observer subject filters which comes with the Qtilities library here:
    // CoreGui filters are installed in QtilitiesApplication
    FactoryItemID activity_policy_filter(qti_def_FACTORY_TAG_ACTIVITY_FILTER,QtilitiesCategory("Subject Filters"));
    d->qtilities_factory.registerFactoryInterface(&ActivityPolicyFilter::factory,activity_policy_filter);
    FactoryItemID subject_type_filter(qti_def_FACTORY_TAG_SUBJECT_TYPE_FILTER,QtilitiesCategory("Subject Filters"));
    d->qtilities_factory.registerFactoryInterface(&SubjectTypeFilter::factory,subject_type_filter);
    FactoryItemID observer(qti_def_FACTORY_TAG_OBSERVER,QtilitiesCategory("Core Classes"));
    d->qtilities_factory.registerFactoryInterface(&Observer::factory,observer);

    // Register the object manager, thus the Qtilities Factory in the list of available IFactories.
    registerIFactoryProvider(this);

    // Register some meta types:
    qRegisterMetaType<Qtilities::Core::QtilitiesCategory>("Qtilities::Core::QtilitiesCategory");

    // Register some stream operators:
    qRegisterMetaTypeStreamOperators<Qtilities::Core::QtilitiesCategory>("Qtilities::Core::QtilitiesCategory");
    qRegisterMetaTypeStreamOperators<Qtilities::Core::SharedProperty>("Qtilities::Core::SharedProperty");
    qRegisterMetaTypeStreamOperators<Qtilities::Core::MultiContextProperty>("Qtilities::Core::MultiContextProperty");
}

Qtilities::Core::ObjectManager::~ObjectManager()
{
    delete d;
}

int Qtilities::Core::ObjectManager::registerObserver(Observer* observer) {
    if (observer) {
        QPointer<Observer> q_pointer = observer;
        d->context_map[d->id] = q_pointer;
        d->id = d->id + 1;
        return d->id-1;
    }

    return -1;
}

QStringList Qtilities::Core::ObjectManager::providedFactories() const {
    QStringList tags;
    tags << qti_def_FACTORY_QTILITIES;
    return tags;
}

QStringList Qtilities::Core::ObjectManager::providedFactoryTags(const QString& factory_name) const {
    if (factory_name == QString(qti_def_FACTORY_QTILITIES))
        return d->qtilities_factory.tags();
    else
        return QStringList();
}

QObject* Qtilities::Core::ObjectManager::createInstance(const InstanceFactoryInfo& ifactory_data) {
    if (ifactory_data.d_factory_tag == QString(qti_def_FACTORY_QTILITIES)) {
        QObject* obj = d->qtilities_factory.createInstance(ifactory_data.d_instance_tag);
        if (obj) {
            return obj;
        }
    }
    return 0;
}

Qtilities::Core::Observer* Qtilities::Core::ObjectManager::observerReference(int id) const {
    if (d->context_map.contains(id)) {
        return d->context_map.value(id);
    } else if (id == 0) {
        return &d->object_pool;
    } else
        return 0;
}

Qtilities::Core::Observer* Qtilities::Core::ObjectManager::objectPool() {
    return &d->object_pool;
}

bool Qtilities::Core::ObjectManager::moveSubjects(QList<QObject*> objects, int source_observer_id, int destination_observer_id, bool silent) {
    // Get observer references
    Observer* source_observer = observerReference(source_observer_id);
    Observer* destination_observer = observerReference(destination_observer_id);

    if (!source_observer || !destination_observer)
        return false;

    bool none_failed = true;

    // For now we discard objects that cause problems during attachment and detachment
    for (int i = 0; i < objects.count(); i++) {
        // Check if the destination observer will accept it
        Observer::EvaluationResult result = destination_observer->canAttach(objects.at(i),Observer::ManualOwnership,0,silent);
        if (result == Observer::Rejected) {           
            break;
        } else {
            // Detach from source
            result = source_observer->canDetach(objects.at(i));
            if (result == Observer::Rejected) {
                LOG_ERROR(QString(QObject::tr("The move operation could not be completed. The object you are trying to move was rejected by the destination observer. Check the session log for more details.")));
                none_failed = false;
                break;
            } else if (result == Observer::IsParentObserver) {
                LOG_ERROR(QString(QObject::tr("The move operation could not be completed. The object you are trying to move cannot be removed from the source observer which is defined to be its owner.\n\nTry to share this object with the destination observer instead.")));
                none_failed = false;
                break;
            } else if (result == Observer::LastScopedObserver) {
                destination_observer->setMultiContextPropertyValue(objects.at(i),qti_prop_OWNERSHIP,QVariant(Observer::ManualOwnership));
                if (!source_observer->detachSubject(objects.at(i))) {
                    destination_observer->setMultiContextPropertyValue(objects.at(i),qti_prop_OWNERSHIP,QVariant(Observer::ObserverScopeOwnership));
                    none_failed = false;
                    break;
                } else {
                    if (!destination_observer->attachSubject(objects.at(i),Observer::ObserverScopeOwnership))
                        source_observer->attachSubject(objects.at(i),Observer::ObserverScopeOwnership);
                }
            } else {
                if (!source_observer->detachSubject(objects.at(i))) {
                    none_failed = false;
                    break;
                } else {
                    if (!destination_observer->attachSubject(objects.at(i)))
                        source_observer->attachSubject(objects.at(i));
                }
            }
        }
    }

    return none_failed;
}

bool Qtilities::Core::ObjectManager::moveSubjects(QList<QPointer<QObject> > objects, int source_observer_id, int destination_observer_id, bool silent) {
    QList<QObject*> simple_objects;
    for (int i = 0; i < objects.count(); i++)
        simple_objects << objects.at(i);
    return moveSubjects(simple_objects,source_observer_id,destination_observer_id,silent);
}

void Qtilities::Core::ObjectManager::registerObject(QObject* obj, QtilitiesCategory category) {
    if (category.isValid() && obj) {
        if (obj->thread() == thread()) {
            if (ObjectManager::propertyExists(obj,qti_prop_CATEGORY_MAP)) {
                MultiContextProperty category_property = ObjectManager::getMultiContextProperty(obj,qti_prop_CATEGORY_MAP);
                category_property.setValue(qVariantFromValue(category),d->object_pool.observerID());
                ObjectManager::setMultiContextProperty(obj,category_property);
            } else {
                MultiContextProperty category_property(qti_prop_CATEGORY_MAP);
                category_property.setValue(qVariantFromValue(category),d->object_pool.observerID());
                ObjectManager::setMultiContextProperty(obj,category_property);
            }
        }
    }
    if (d->object_pool.attachSubject(obj))
        emit newObjectAdded(obj);
}

void Qtilities::Core::ObjectManager::removeObject(QObject* obj) {
    d->object_pool.detachSubject(obj);
}

void Qtilities::Core::ObjectManager::registerFactoryInterface(FactoryInterface<QObject>* interface, FactoryItemID iface_tag) {
    d->qtilities_factory.registerFactoryInterface(interface,iface_tag);
}

bool Qtilities::Core::ObjectManager::registerIFactoryProvider(IFactoryProvider* factory_iface) {
    if (!factory_iface)
        return false;

    // Check if all factory names provided through factory_iface are unique first:
    foreach(QString name, factory_iface->providedFactories()) {
        if (d->factory_map.keys().contains(name)) {
            LOG_ERROR(QString(tr("Object Manager: Ambiguous factory name \"%1\" detected in registerIFactoryProvider(). This IFactoryProvider interface will not be registered.")).arg(name));
            return false;
        }
    }

    foreach(QString name, factory_iface->providedFactories()) {
        if (!d->factory_map.keys().contains(name)) {
            d->factory_map[name] = factory_iface;
        }
    }

    return true;
}

Qtilities::Core::Interfaces::IFactoryProvider* Qtilities::Core::ObjectManager::referenceIFactoryProvider(const QString& tag) const {
    if (d->factory_map.contains(tag))
        return d->factory_map[tag];
    else
        return 0;
}

QStringList Qtilities::Core::ObjectManager::allFactoryNames() const {
    QStringList names;
    QStringList ifactory_keys = d->factory_map.keys();

    foreach (QString ifactory_key, ifactory_keys) {
        IFactoryProvider* ifactory = referenceIFactoryProvider(ifactory_key);
        if (ifactory)
            names << ifactory->providedFactories();
    }

    return names;
}

QStringList Qtilities::Core::ObjectManager::tagsForFactory(const QString& factory_name) const {
    QStringList tags;
    QStringList ifactory_keys = d->factory_map.keys();

    foreach (QString ifactory_key, ifactory_keys) {
        IFactoryProvider* ifactory = referenceIFactoryProvider(ifactory_key);
        if (ifactory) {
            foreach (QString factory_name_int, ifactory->providedFactories()) {
                if (factory_name_int == factory_name) {
                    tags << ifactory->providedFactoryTags(factory_name);
                    break;
                }
            }
        }
    }

    return tags;
}

QList<QObject*> Qtilities::Core::ObjectManager::registeredInterfaces(const QString& iface) const {
    return d->object_pool.subjectReferences(iface);
}

// TODO: Remove this:
void Qtilities::Core::ObjectManager::setMetaTypeActiveObjects(QList<QObject*> objects, const QString& meta_type) {
    QList<QPointer<QObject> > smart_objects;
    for (int i = 0; i < objects.count(); i++)
        smart_objects << objects.at(i);

    d->meta_type_map[meta_type] = smart_objects;
    emit metaTypeActiveObjectsChanged(smart_objects,meta_type);
}

void Qtilities::Core::ObjectManager::setMetaTypeActiveObjects(QList<QPointer<QObject> > objects, const QString& meta_type) {
    d->meta_type_map[meta_type] = objects;
    emit metaTypeActiveObjectsChanged(objects,meta_type);
}

QList<QPointer<QObject> > Qtilities::Core::ObjectManager::metaTypeActiveObjects(const QString& meta_type) const {
    return d->meta_type_map[meta_type];
}

// --------------------------------
// Static Dynamic Property Functions
// --------------------------------

quint32 MARKER_PROPERTY_EXPORT = 0xDEADBEEF;

IExportable::Result Qtilities::Core::ObjectManager::exportObjectPropertiesBinary(const QObject* obj, QDataStream& stream, PropertyTypeFlags property_types, Qtilities::ExportVersion version) {
    if (!obj)
        return IExportable::Failed;

    bool is_supported_format = false;
    if (version == Qtilities::Qtilities_1_0)
        is_supported_format = true;

    if (!is_supported_format) {
        LOG_ERROR(QString(tr("Unsupported property stream version (%1) requested on object. The properties on this object will not be parsed.")).arg(version));
        return IExportable::Failed;
    }

    // Get all properties to be exported:
    QList<SharedProperty> properties_shared;
    QList<MultiContextProperty> properties_multi_context;
    QMap<QString,QVariant> properties_normal;
    for (int i = 0; i < obj->dynamicPropertyNames().count(); i++) {
        QString property_name = QString(obj->dynamicPropertyNames().at(i).constData());
        if (!(property_types & ObjectManager::QtilitiesInternalProperties) && property_name.startsWith("qti."))
            continue;

        // Now check the property types:
        QVariant prop = obj->property(obj->dynamicPropertyNames().at(i).constData());
        if (prop.isValid() && prop.canConvert<SharedProperty>() && (property_types & ObjectManager::SharedProperties)) {
            SharedProperty shared_prop = prop.value<SharedProperty>();
            shared_prop.setExportVersion(version);
            properties_shared << shared_prop;
        } else if (prop.isValid() && prop.canConvert<MultiContextProperty>() && (property_types & ObjectManager::MultiContextProperties)) {
            MultiContextProperty multi_context_prop = prop.value<MultiContextProperty>();
            multi_context_prop.setExportVersion(version);
            properties_multi_context << multi_context_prop;
        } else if (property_types & ObjectManager::NonQtilitiesProperties) {
            properties_normal[QString(obj->dynamicPropertyNames().at(i).constData())] = prop;
        }
    }

    // TODO add export format to v1.0 export formats docs.
    // Now export the properties:
    stream << MARKER_PROPERTY_EXPORT;
    stream << (quint32) version;
    stream << QtilitiesCoreApplication::qtilitiesVersionString();

    if (version == Qtilities::Qtilities_1_0) {
        // Shared Properties:
        stream << (quint32) properties_shared.count();
        stream << properties_shared;

        // Multi Context Properties:
        stream << (quint32) properties_multi_context.count();
        stream << properties_multi_context;

        // Normal QVariant Properties:
        stream << (quint32) properties_normal.count();
        stream << properties_normal;
     }

    stream << MARKER_PROPERTY_EXPORT;

    return IExportable::Complete;
}

IExportable::Result Qtilities::Core::ObjectManager::importObjectPropertiesBinary(QObject* obj, QDataStream& stream) {
    if (!obj)
        return IExportable::Failed;

    quint32 marker;
    stream >> marker;
    if (marker != MARKER_PROPERTY_EXPORT) {
        LOG_ERROR(QString(tr("Failed to load properties on object. Missing property marker at beginning of data stream.")));
        return IExportable::Failed;
    }

    stream >> marker;
    Qtilities::ExportVersion read_version = (Qtilities::ExportVersion) marker;
    QString qtilities_version;
    stream >> qtilities_version;

    bool is_supported_format = false;
    if (read_version == Qtilities::Qtilities_1_0)
        is_supported_format = true;

    if (!is_supported_format) {
        LOG_ERROR(QString(tr("Unsupported property stream found with export version: %1. The properties on this object will not be parsed.")).arg(read_version));
        return IExportable::Failed;
    }

    QList<SharedProperty> properties_shared;
    QList<MultiContextProperty> properties_multi_context;
    QMap<QString,QVariant> properties_normal;

    if (read_version == Qtilities::Qtilities_1_0) {
        quint32 count;
        stream >> count;
        if (count > 0) {
            stream >> properties_shared;
        }
        stream >> count;
        if (count > 0) {
            stream >> properties_multi_context;
        }
        stream >> count;
        if (count > 0) {
            stream >> properties_normal;
        }
    }

    stream >> marker;
    if (marker != MARKER_PROPERTY_EXPORT) {
        LOG_ERROR(QString(tr("Failed to load properties on object. Missing property marker at beginning of data stream.")));
        return IExportable::Failed;
    }

    for (int i = 0; i < properties_shared.count(); i++) {
        if (!(ObjectManager::setSharedProperty(obj,properties_shared.at(i))))
            return IExportable::Failed;
    }
    for (int i = 0; i < properties_multi_context.count(); i++) {
        if (!(ObjectManager::setMultiContextProperty(obj,properties_multi_context.at(i))))
            return IExportable::Failed;
    }
    for (int i = 0; i < properties_normal.count(); i++) {
        obj->setProperty(properties_normal.keys().at(i).toUtf8().constData(),properties_normal.values().at(i));
    }

    return IExportable::Complete;
}

IExportable::Result Qtilities::Core::ObjectManager::exportObjectPropertiesXml(const QObject* obj, QDomDocument* doc, QDomElement* object_node, PropertyTypeFlags property_types, Qtilities::ExportVersion version) {
    if (!obj)
        return IExportable::Failed;

    bool is_supported_format = false;
    if (version == Qtilities::Qtilities_1_0)
        is_supported_format = true;

    if (!is_supported_format) {
        LOG_ERROR(QString(tr("Unsupported property xml export version (%1) requested on object. The properties on this object will not be parsed.")).arg(version));
        return IExportable::Failed;
    }

    // Get all properties to be exported:
    QList<SharedProperty> properties_shared;
    QList<MultiContextProperty> properties_multi_context;
    QMap<QString,QVariant> properties_normal;
    for (int i = 0; i < obj->dynamicPropertyNames().count(); i++) {
        QString property_name = QString(obj->dynamicPropertyNames().at(i).constData());
        if (!(property_types & ObjectManager::QtilitiesInternalProperties) && property_name.startsWith("qti."))
            continue;

        // Now check the property types:
        QVariant prop = obj->property(obj->dynamicPropertyNames().at(i).constData());
        if (prop.isValid() && prop.canConvert<SharedProperty>() && (property_types & ObjectManager::SharedProperties)) {
            SharedProperty shared_prop = prop.value<SharedProperty>();
            shared_prop.setExportVersion(version);
            properties_shared << shared_prop;
        } else if (prop.isValid() && prop.canConvert<MultiContextProperty>() && (property_types & ObjectManager::MultiContextProperties)) {
            MultiContextProperty multi_context_prop = prop.value<MultiContextProperty>();
            multi_context_prop.setExportVersion(version);
            properties_multi_context << multi_context_prop;
        } else if (property_types & ObjectManager::NonQtilitiesProperties) {
            properties_normal[QString(obj->dynamicPropertyNames().at(i).constData())] = prop;
        }
    }

    // Now do the export:
    QDomElement property_node = doc->createElement("Properties");
    object_node->appendChild(property_node);

    // Some export format information:
    property_node.setAttribute("ExportVersion",QString::number(version));
    property_node.setAttribute("QtilitiesVersion",QtilitiesCoreApplication::qtilitiesVersionString());

    IExportable::Result result = IExportable::Complete;

    // Save all the properties:
    if (version == Qtilities::Qtilities_1_0) {
        // Shared Properties:
        property_node.setAttribute("PropertyS_Count",properties_shared.count());
        for (int i = 0; i < properties_shared.count(); i++) {
            QDomElement current_node = doc->createElement("PropertyS_" + QString::number(i));
            property_node.appendChild(current_node);

            IExportable::Result intermediate_result = properties_shared.at(i).exportXml(doc,&current_node);
            if (intermediate_result == IExportable::Failed)
                return intermediate_result;

            if (intermediate_result == IExportable::Incomplete)
                result = intermediate_result;
        }

        // Multi Context Properties:
        property_node.setAttribute("PropertyM_Count",properties_multi_context.count());
        for (int i = 0; i < properties_multi_context.count(); i++) {
            QDomElement current_node = doc->createElement("PropertyM_" + QString::number(i));
            property_node.appendChild(current_node);

            IExportable::Result intermediate_result = properties_multi_context.at(i).exportXml(doc,&current_node);
            if (intermediate_result == IExportable::Failed)
                return intermediate_result;

            if (intermediate_result == IExportable::Incomplete)
                result = intermediate_result;
        }

        // Normal QVariant Properties:
        property_node.setAttribute("PropertyN_Count",properties_normal.count());
        for (int i = 0; i < properties_normal.count(); i++) {
            QDomElement current_node = doc->createElement("PropertyN");
            property_node.appendChild(current_node);

            current_node.setAttribute("Type",properties_normal.values().at(i).typeName());
            current_node.setAttribute("Name",properties_normal.keys().at(i));
            if (properties_normal.values().at(i).type() == QVariant::StringList)
                current_node.setAttribute("Value",properties_normal.values().at(i).toStringList().join(","));
            else
                current_node.setAttribute("Value",properties_normal.values().at(i).toString());
        }
     }

    return result;
}

IExportable::Result Qtilities::Core::ObjectManager::importObjectPropertiesXml(QObject* obj, QDomDocument* doc, QDomElement* object_node) {
    if (!obj)
        return IExportable::Failed;

    QList<SharedProperty> properties_shared;
    QList<MultiContextProperty> properties_multi_context;
    QMap<QString,QVariant> properties_normal;

    IExportable::Result result = IExportable::Complete;

    QDomNodeList itemNodes = object_node->childNodes();
    for(int i = 0; i < itemNodes.count(); i++) {
        QDomNode itemNode = itemNodes.item(i);
        QDomElement item = itemNode.toElement();

        if (item.isNull())
            continue;

        if (item.tagName().startsWith("Properties")) {
            // ---------------------------------------------------
            // Inspect file format:
            // ---------------------------------------------------
            Qtilities::ExportVersion read_version;
            if (item.hasAttribute("ExportVersion")) {
                read_version = (Qtilities::ExportVersion) item.attribute("ExportVersion").toInt();
            } else {
                LOG_ERROR(QString(tr("The export version of the xml property stream could not be determined. The properties for this object will not be parsed.")));
                return IExportable::Failed;
            }
            if (!item.hasAttribute("QtilitiesVersion")) {
                LOG_ERROR(QString(tr("The qtilities export version of the xml property stream could not be determined. The properties for this object will not be parsed.")));
                return IExportable::Failed;
            }

            bool is_supported_format = false;
            if (read_version == Qtilities::Qtilities_1_0)
                is_supported_format = true;

            if (!is_supported_format) {
                LOG_ERROR(QString(tr("Unsupported property xml stream found with export version: %1. The properties for this object will not be parsed.")).arg(read_version));
                return IExportable::Failed;
            }

            if (read_version == Qtilities::Qtilities_1_0) {
                QList<QPointer<QObject> > import_list;

                QDomNodeList propertyNodes = item.childNodes();
                for(int i = 0; i < propertyNodes.count(); i++) {
                    QDomNode propertyNode = propertyNodes.item(i);
                    QDomElement property = propertyNode.toElement();

                    if (property.isNull())
                        continue;

                    if (property.tagName().startsWith("PropertyS")) {
                        SharedProperty prop;
                        IExportable::Result intermediate_result = prop.importXml(doc,&property,import_list);
                        if (intermediate_result == IExportable::Failed)
                            return intermediate_result;
                        if (intermediate_result == IExportable::Incomplete)
                            result = intermediate_result;

                        properties_shared << prop;

                        continue;
                    } else if (property.tagName().startsWith("PropertyM")) {
                        MultiContextProperty prop;
                        IExportable::Result intermediate_result = prop.importXml(doc,&property,import_list);
                        if (intermediate_result == IExportable::Failed)
                            return intermediate_result;
                        if (intermediate_result == IExportable::Incomplete)
                            result = intermediate_result;

                        properties_multi_context << prop;

                        continue;
                    } else if (property.tagName().startsWith("PropertyN")) {
                        if (property.hasAttribute("Name") && property.hasAttribute("Type") && property.hasAttribute("Value")) {
                            QString property_name = property.attribute("Name");
                            QString property_type = property.attribute("Type");
                            QString property_value = property.attribute("Value");
                            properties_normal[property_name] = QtilitiesProperty::constructVariant(property_type,property_value);
                        } else
                            result = IExportable::Incomplete;

                        continue;
                    }
                }
            }
            continue;
        } else
            return IExportable::Incomplete;
    }

    for (int i = 0; i < properties_shared.count(); i++) {
        if (!(ObjectManager::setSharedProperty(obj,properties_shared.at(i))))
            return IExportable::Failed;
    }
    for (int i = 0; i < properties_multi_context.count(); i++) {
        if (!(ObjectManager::setMultiContextProperty(obj,properties_multi_context.at(i))))
            return IExportable::Failed;
    }
    for (int i = 0; i < properties_normal.count(); i++) {
        obj->setProperty(properties_normal.keys().at(i).toUtf8().constData(),properties_normal.values().at(i));
    }

    return result;
}

bool Qtilities::Core::ObjectManager::cloneObjectProperties(const QObject* source_obj, QObject* target_obj, PropertyTypeFlags property_types) {
    if (!target_obj || !source_obj)
        return false;

    // Get all properties from source_obj:
    QList<SharedProperty> shared_properties;
    QList<MultiContextProperty> multi_context_properties;
    QMap<QString,QVariant> non_qtilities_properties;

    for (int i = 0; i < source_obj->dynamicPropertyNames().count(); i++) {
        QString property_name = QString(source_obj->dynamicPropertyNames().at(i).constData());
        if (!(property_types & ObjectManager::QtilitiesInternalProperties) && property_name.startsWith("qti."))
            continue;

        // Now check the property types:
        QVariant prop = source_obj->property(source_obj->dynamicPropertyNames().at(i).constData());
        if (prop.isValid() && prop.canConvert<SharedProperty>() && (property_types & ObjectManager::SharedProperties))
            shared_properties << prop.value<SharedProperty>();
        else if (prop.isValid() && prop.canConvert<MultiContextProperty>() && (property_types & ObjectManager::MultiContextProperties))
            multi_context_properties << prop.value<MultiContextProperty>();
        else if (property_types & ObjectManager::NonQtilitiesProperties) {
            non_qtilities_properties[property_name] = prop;
        }
    }

    // Now that we have all the properties, we add them to the target obj:
    for (int i = 0; i < shared_properties.count(); i++) {
        ObjectManager::setSharedProperty(target_obj,shared_properties.at(i));
    }
    LOG_TRACE(QString("Cloning %1 shared properties from object %2 to object %3.").arg(shared_properties.count()).arg(source_obj->objectName()).arg(target_obj->objectName()));

    for (int i = 0; i < multi_context_properties.count(); i++) {
        ObjectManager::setMultiContextProperty(target_obj,multi_context_properties.at(i));
    }
    LOG_TRACE(QString("Cloning %1 multi context properties from object %2 to object %3.").arg(multi_context_properties.count()).arg(source_obj->objectName()).arg(target_obj->objectName()));

    for (int i = 0; i < non_qtilities_properties.count(); i++) {
        target_obj->setProperty(non_qtilities_properties.keys().at(i).toLocal8Bit().data(),non_qtilities_properties.values().at(i));
    }
    LOG_TRACE(QString("Cloning %1 non-qtilities properties from object %2 to object %3").arg(non_qtilities_properties.count()).arg(source_obj->objectName()).arg(target_obj->objectName()));

    return true;
}

Qtilities::Core::MultiContextProperty Qtilities::Core::ObjectManager::getMultiContextProperty(const QObject* obj, const char* property_name) {
    #ifndef QT_NO_DEBUG
        if (!obj)
            qDebug() << "Failed to get multi-context property \"" << property_name  << "\" on null object";
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return MultiContextProperty();
    #endif

    QVariant prop = obj->property(property_name);
    if (prop.isValid() && prop.canConvert<MultiContextProperty>())
        return prop.value<MultiContextProperty>();
    else
        return MultiContextProperty();
}

bool Qtilities::Core::ObjectManager::setMultiContextProperty(QObject* obj, MultiContextProperty multi_context_property) {
    #ifndef QT_NO_DEBUG
        if (!obj)
            qDebug() << "Failed to set multi context property \"" << multi_context_property.propertyNameString() << "\" on null object";
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return false;
    #endif

    if (!multi_context_property.isValid()) {
        qDebug() << "Failed to set multi context property on object, the property is invalid.";
        Q_ASSERT(multi_context_property.isValid());
        return false;
    }

    QVariant property = qVariantFromValue(multi_context_property);
    return !obj->setProperty(multi_context_property.propertyNameString().toAscii().data(),property);
}

Qtilities::Core::SharedProperty Qtilities::Core::ObjectManager::getSharedProperty(const QObject* obj, const char* property_name) {
    #ifndef QT_NO_DEBUG
        if (!obj)
            qDebug() << "Failed to get shared property \"" << property_name << "\" on null object";
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return SharedProperty();
    #endif

    QVariant prop = obj->property(property_name);
    if (prop.isValid() && prop.canConvert<SharedProperty>())
        return prop.value<SharedProperty>();
    else
        return SharedProperty();
}

bool Qtilities::Core::ObjectManager::setSharedProperty(QObject* obj, SharedProperty shared_property) {
    #ifndef QT_NO_DEBUG
        if (!obj)
            qDebug() << "Failed to set shared property \"" << shared_property.propertyNameString() << "\" on null object";
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return false;
    #endif

    if (!shared_property.isValid()) {
        qDebug() << "Failed to set shared property on object, the property is invalid.";
        Q_ASSERT(shared_property.isValid());
        return false;
    }

    QVariant property = qVariantFromValue(shared_property);
    return !obj->setProperty(shared_property.propertyNameString().toAscii().data(),property);
}

bool Qtilities::Core::ObjectManager::propertyExists(const QObject* obj, const char* property_name) {
    if (!obj)
        return false;

    QVariant prop = obj->property(property_name);
    return prop.isValid();
}

bool Qtilities::Core::ObjectManager::removeDynamicProperties(QObject* obj, PropertyTypeFlags property_types) {
    if (!obj)
        return false;

    // Get all properties from obj:
    QList<QString> to_be_removed;

    for (int i = 0; i < obj->dynamicPropertyNames().count(); i++) {
        QString property_name = QString(obj->dynamicPropertyNames().at(i).constData());
        if (!(property_types & ObjectManager::QtilitiesInternalProperties) && property_name.startsWith("qti."))
            continue;

        // Now check the property types:
        QVariant prop = obj->property(obj->dynamicPropertyNames().at(i).constData());
        if (prop.isValid() && prop.canConvert<SharedProperty>() && (property_types & ObjectManager::SharedProperties))
            to_be_removed << QString(obj->dynamicPropertyNames().at(i).constData());
        else if (prop.isValid() && prop.canConvert<MultiContextProperty>() && (property_types & ObjectManager::MultiContextProperties))
            to_be_removed << QString(obj->dynamicPropertyNames().at(i).constData());
        else if (property_types & ObjectManager::NonQtilitiesProperties) {
            to_be_removed << QString(obj->dynamicPropertyNames().at(i).constData());
        }
    }

    LOG_TRACE(QString("Removing %1 dynamic properties from object %2.").arg(to_be_removed.count()).arg(obj->objectName()));

    foreach (QString prop_name, to_be_removed)
        obj->setProperty(prop_name.toUtf8().constData(),QVariant());

    return true;
}

bool Qtilities::Core::ObjectManager::compareDynamicProperties(const QObject* obj1, const QObject* obj2, PropertyTypeFlags property_types) {
    if (!obj1 || !obj2)
        return false;

    // Get a list of properties on obj1 which must be used in comparison:
    QMap<QString, QVariant> to_be_compared_normal1;
    QMap<QString, SharedProperty> to_be_compared_shared1;
    QMap<QString, MultiContextProperty> to_be_compared_multi1;
    for (int i = 0; i < obj1->dynamicPropertyNames().count(); i++) {
        QString property_name = QString(obj1->dynamicPropertyNames().at(i).constData());
        if (!(property_types & ObjectManager::QtilitiesInternalProperties) && property_name.startsWith("qti."))
            continue;

        // Now check the property types:
        QVariant prop = obj1->property(obj1->dynamicPropertyNames().at(i).constData());
        if (prop.isValid() && prop.canConvert<SharedProperty>() && (property_types & ObjectManager::SharedProperties))
            to_be_compared_shared1[QString(obj1->dynamicPropertyNames().at(i).constData())] = prop.value<SharedProperty>();
        else if (prop.isValid() && prop.canConvert<MultiContextProperty>() && (property_types & ObjectManager::MultiContextProperties))
            to_be_compared_multi1[QString(obj1->dynamicPropertyNames().at(i).constData())] = prop.value<MultiContextProperty>();
        else if (property_types & ObjectManager::NonQtilitiesProperties) {
            to_be_compared_normal1[QString(obj1->dynamicPropertyNames().at(i).constData())] = prop;
        }
    }

    // Get a list of properties on obj2 which must be used in comparison:
    QMap<QString, QVariant> to_be_compared_normal2;
    QMap<QString, SharedProperty> to_be_compared_shared2;
    QMap<QString, MultiContextProperty> to_be_compared_multi2;
    for (int i = 0; i < obj2->dynamicPropertyNames().count(); i++) {
        QString property_name = QString(obj2->dynamicPropertyNames().at(i).constData());
        if (!(property_types & ObjectManager::QtilitiesInternalProperties) && property_name.startsWith("qti."))
            continue;

        // Now check the property types:
        QVariant prop = obj2->property(obj2->dynamicPropertyNames().at(i).constData());
        if (prop.isValid() && prop.canConvert<SharedProperty>() && (property_types & ObjectManager::SharedProperties))
            to_be_compared_shared2[QString(obj2->dynamicPropertyNames().at(i).constData())] = prop.value<SharedProperty>();
        else if (prop.isValid() && prop.canConvert<MultiContextProperty>() && (property_types & ObjectManager::MultiContextProperties))
            to_be_compared_multi2[QString(obj2->dynamicPropertyNames().at(i).constData())] = prop.value<MultiContextProperty>();
        else if (property_types & ObjectManager::NonQtilitiesProperties) {
            to_be_compared_normal2[QString(obj2->dynamicPropertyNames().at(i).constData())] = prop;
        }
    }

    // Compare lists of objects to compare:
    if (to_be_compared_normal1 != to_be_compared_normal2) {
        LOG_TRACE(QString("Comparing dynamic properties on object %1 with object %2. Comparison found that normal QVariant properties differ.").arg(obj1->objectName()).arg(obj2->objectName()));
        return false;
    }
    if (to_be_compared_shared1 != to_be_compared_shared2) {
        LOG_TRACE(QString("Comparing dynamic properties on object %1 with object %2. Comparison found that shared properties differ.").arg(obj1->objectName()).arg(obj2->objectName()));
        return false;
    }
    if (to_be_compared_multi1 != to_be_compared_multi2) {
        LOG_TRACE(QString("Comparing dynamic properties on object %1 with object %2. Comparison found that multi context properties differ.").arg(obj1->objectName()).arg(obj2->objectName()));
        return false;
    }

    return true;
}

