/***************************************************************************
 *   Copyright (c) 2007 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#include <Base/FileInfo.h>
#include <Base/Interpreter.h>
#include <Base/Stream.h>

#include "Document.h"
#include "DocumentObject.h"
#include "DocumentObjectPy.h"
#include "MergeDocuments.h"

// inclusion of the generated files (generated By DocumentPy.xml)
#include "DocumentPy.h"
#include "DocumentPy.cpp"
#include <boost/regex.hpp>
#include <Base/PyWrapParseTupleAndKeywords.h>

using namespace App;


PyObject*  DocumentPy::addProperty(PyObject *args, PyObject *kwd)
{
    char *sType,*sName=nullptr,*sGroup=nullptr,*sDoc=nullptr;
    short attr=0;
    std::string sDocStr;
    PyObject *ro = Py_False, *hd = Py_False;
    PyObject* enumVals = nullptr;
    static char *kwlist[] = {"type","name","group","doc","attr","read_only","hidden","enum_vals",nullptr};
    if (!PyArg_ParseTupleAndKeywords(
            args, kwd, "ss|sethO!O!O", kwlist, &sType, &sName, &sGroup, "utf-8",
            &sDoc, &attr, &PyBool_Type, &ro, &PyBool_Type, &hd, &enumVals))
        return nullptr;

    if (sDoc) {
        sDocStr = sDoc;
        PyMem_Free(sDoc);
    }

    Property *prop = getDocumentPtr()->
        addDynamicProperty(sType,sName,sGroup,sDocStr.c_str(),attr,
                           Base::asBoolean(ro), Base::asBoolean(hd));

    // enum support
    auto* propEnum = dynamic_cast<App::PropertyEnumeration*>(prop);
    if (propEnum) {
        if (enumVals && PySequence_Check(enumVals)) {
            std::vector<std::string> enumValsAsVector;
            for (Py_ssize_t i = 0; i < PySequence_Length(enumVals); ++i) {
                enumValsAsVector.emplace_back(PyUnicode_AsUTF8(PySequence_GetItem(enumVals,i)));
            }
            propEnum->setEnums(enumValsAsVector);
        }
    }

    return Py::new_reference_to(this);
}

PyObject*  DocumentPy::removeProperty(PyObject *args)
{
    char *sName;
    if (!PyArg_ParseTuple(args, "s", &sName))
        return nullptr;

    bool ok = getDocumentPtr()->removeDynamicProperty(sName);
    return Py_BuildValue("O", (ok ? Py_True : Py_False));
}

// returns a string which represent the object e.g. when printed in python
std::string DocumentPy::representation() const
{
    std::stringstream str;
    str << "<Document object at " << getDocumentPtr() << ">";

    return str.str();
}

PyObject*  DocumentPy::save(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    PY_TRY {
        if (!getDocumentPtr()->save()) {
            PyErr_SetString(PyExc_ValueError, "Object attribute 'FileName' is not set");
            return nullptr;
        }
    } PY_CATCH;

    const char* filename = getDocumentPtr()->FileName.getValue();
    Base::FileInfo fi(filename);
    if (!fi.isReadable()) {
        PyErr_Format(PyExc_IOError, "No such file or directory: '%s'", filename);
        return nullptr;
    }

    Py_Return;
}

PyObject*  DocumentPy::saveAs(PyObject * args)
{
    char* fn;
    if (!PyArg_ParseTuple(args, "et", "utf-8", &fn))
        return nullptr;

    std::string utf8Name = fn;
    PyMem_Free(fn);

    PY_TRY {
        getDocumentPtr()->saveAs(utf8Name.c_str());
        Py_Return;
    }PY_CATCH
}

PyObject*  DocumentPy::saveCopy(PyObject * args)
{
    char* fn;
    if (!PyArg_ParseTuple(args, "s", &fn))
        return nullptr;

    PY_TRY {
        getDocumentPtr()->saveCopy(fn);
        Py_Return;
    }PY_CATCH
}

PyObject*  DocumentPy::load(PyObject * args)
{
    char* filename=nullptr;
    if (!PyArg_ParseTuple(args, "s", &filename))
        return nullptr;
    if (!filename || *filename == '\0') {
        PyErr_Format(PyExc_ValueError, "Path is empty");
        return nullptr;
    }

    getDocumentPtr()->FileName.setValue(filename);
    Base::FileInfo fi(filename);
    if (!fi.isReadable()) {
        PyErr_Format(PyExc_IOError, "No such file or directory: '%s'", filename);
        return nullptr;
    }
    try {
        getDocumentPtr()->restore();
    } catch (...) {
        PyErr_Format(PyExc_IOError, "Reading from file '%s' failed", filename);
        return nullptr;
    }
    Py_Return;
}

PyObject*  DocumentPy::restore(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    const char* filename = getDocumentPtr()->FileName.getValue();
    if (!filename || *filename == '\0') {
        PyErr_Format(PyExc_ValueError, "Object attribute 'FileName' is not set");
        return nullptr;
    }
    Base::FileInfo fi(filename);
    if (!fi.isReadable()) {
        PyErr_Format(PyExc_IOError, "No such file or directory: '%s'", filename);
        return nullptr;
    }
    try {
        getDocumentPtr()->restore();
    } catch (...) {
        PyErr_Format(PyExc_IOError, "Reading from file '%s' failed", filename);
        return nullptr;
    }
    Py_Return;
}

PyObject* DocumentPy::isSaved(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    bool ok = getDocumentPtr()->isSaved();
    return Py::new_reference_to(Py::Boolean(ok));
}

PyObject* DocumentPy::getProgramVersion(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    const char* version = getDocumentPtr()->getProgramVersion();
    return Py::new_reference_to(Py::String(version));
}

PyObject* DocumentPy::getFileName(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    const char* fn = getDocumentPtr()->getFileName();
    return Py::new_reference_to(Py::String(fn));
}

PyObject*  DocumentPy::mergeProject(PyObject * args)
{
    char* filename;
    if (!PyArg_ParseTuple(args, "s", &filename))
        return nullptr;

    PY_TRY {
        Base::FileInfo fi(filename);
        Base::ifstream str(fi, std::ios::in | std::ios::binary);
        App::Document* doc = getDocumentPtr();
        MergeDocuments md(doc);
        md.importObjects(str);
        Py_Return;
    } PY_CATCH;
}

PyObject*  DocumentPy::exportGraphviz(PyObject * args)
{
    char* fn=nullptr;
    if (!PyArg_ParseTuple(args, "|s",&fn))
        return nullptr;
    if (fn) {
        Base::FileInfo fi(fn);
        Base::ofstream str(fi);
        getDocumentPtr()->exportGraphviz(str);
        str.close();
        Py_Return;
    }
    else {
        std::stringstream str;
        getDocumentPtr()->exportGraphviz(str);
        return PyUnicode_FromString(str.str().c_str());
    }
}

PyObject*  DocumentPy::addObject(PyObject *args, PyObject *kwd)
{
    char *sType, *sName = nullptr, *sViewType = nullptr;
    PyObject *obj = nullptr;
    PyObject *view = nullptr;
    PyObject *attach = Py_False;
    static const std::array<const char *, 7> kwlist{"type", "name", "objProxy", "viewProxy", "attach", "viewType",
                                                    nullptr};
    if (!Base::Wrapped_ParseTupleAndKeywords(args, kwd, "s|sOOO!s",
                                             kwlist, &sType, &sName, &obj, &view, &PyBool_Type, &attach, &sViewType)) {
        return nullptr;
    }

    DocumentObject *pcFtr = nullptr;

    if (!obj || !Base::asBoolean(attach)) {
        pcFtr = getDocumentPtr()->addObject(sType,sName,true,sViewType);
    }
    else {
        Base::Type type = Base::Type::getTypeIfDerivedFrom(sType, DocumentObject::getClassTypeId(), true);
        if (type.isBad()) {
            std::stringstream str;
            str << "'" << sType << "' is not a document object type";
            throw Base::TypeError(str.str());
        }
        pcFtr = static_cast<DocumentObject*>(type.createInstance());
    }
    // the type instance could be a null pointer
    if (!pcFtr) {
        std::stringstream str;
        str << "No document object found of type '" << sType << "'" << std::ends;
        throw Py::TypeError(str.str());
    }
    // Allows to hide the handling with Proxy in client python code
    if (obj) {
        try {
            // the python binding class to the document object
            Py::Object pyftr = Py::asObject(pcFtr->getPyObject());
            // 'pyobj' is the python class with the implementation for DocumentObject
            Py::Object pyobj(obj);
            if (pyobj.hasAttr("__object__")) {
                pyobj.setAttr("__object__", pyftr);
            }
            pyftr.setAttr("Proxy", pyobj);

            if (Base::asBoolean(attach)) {
                getDocumentPtr()->addObject(pcFtr,sName);

                try {
                    Py::Callable method(pyobj.getAttr("attach"));
                    if (!method.isNone()) {
                        Py::TupleN arg(pyftr);
                        method.apply(arg);
                    }
                }
                catch (Py::Exception&) {
                    Base::PyException e;
                    e.ReportException();
                }
            }

            // if a document class is set we also need a view provider defined which must be
            // something different to None
            Py::Object pyvp;
            if (view)
                pyvp = Py::Object(view);
            if (pyvp.isNone())
                pyvp = Py::Int(1);
            // 'pyvp' is the python class with the implementation for ViewProvider
            if (pyvp.hasAttr("__vobject__")) {
                pyvp.setAttr("__vobject__", pyftr.getAttr("ViewObject"));
            }

            Py::Object pyprx(pyftr.getAttr("ViewObject"));
            pyprx.setAttr("Proxy", pyvp);
            return Py::new_reference_to(pyftr);
        }
        catch (Py::Exception& e) {
            e.clear();
        }
    }
    return pcFtr->getPyObject();
}

PyObject*  DocumentPy::removeObject(PyObject *args)
{
    char *sName;
    if (!PyArg_ParseTuple(args, "s",&sName))
        return nullptr;


    DocumentObject *pcFtr = getDocumentPtr()->getObject(sName);
    if (pcFtr) {
        getDocumentPtr()->removeObject( sName );
        Py_Return;
    }
    else {
        std::stringstream str;
        str << "No document object found with name '" << sName << "'" << std::ends;
        throw Py::ValueError(str.str());
    }
}

PyObject*  DocumentPy::copyObject(PyObject *args)
{
    PyObject *obj, *rec=Py_False, *retAll=Py_False;
    if (!PyArg_ParseTuple(args, "O|O!O!",&obj,&PyBool_Type,&rec,&PyBool_Type,&retAll))
        return nullptr;

    std::vector<App::DocumentObject*> objs;
    bool single = false;
    if (PySequence_Check(obj)) {
        Py::Sequence seq(obj);
        for (Py_ssize_t i=0;i<seq.size();++i) {
            if (!PyObject_TypeCheck(seq[i].ptr(),&DocumentObjectPy::Type)) {
                PyErr_SetString(PyExc_TypeError, "Expect element in sequence to be of type document object");
                return nullptr;
            }
            objs.push_back(static_cast<DocumentObjectPy*>(seq[i].ptr())->getDocumentObjectPtr());
        }
    }
    else if (!PyObject_TypeCheck(obj,&DocumentObjectPy::Type)) {
        PyErr_SetString(PyExc_TypeError,
            "Expect first argument to be either a document object or sequence of document objects");
        return nullptr;
    }
    else {
        objs.push_back(static_cast<DocumentObjectPy*>(obj)->getDocumentObjectPtr());
        single = true;
    }

    PY_TRY {
        auto ret = getDocumentPtr()->copyObject(objs, Base::asBoolean(rec), Base::asBoolean(retAll));
        if (ret.size()==1 && single)
            return ret[0]->getPyObject();

        Py::Tuple tuple(ret.size());
        for (size_t i=0;i<ret.size();++i)
            tuple.setItem(i,Py::Object(ret[i]->getPyObject(),true));
        return Py::new_reference_to(tuple);
    } PY_CATCH
}

PyObject*  DocumentPy::importLinks(PyObject *args)
{
    PyObject *obj = Py_None;
    if (!PyArg_ParseTuple(args, "|O",&obj))
        return nullptr;

    std::vector<App::DocumentObject*> objs;
    if (PySequence_Check(obj)) {
        Py::Sequence seq(obj);
        for (Py_ssize_t i=0;i<seq.size();++i) {
            if (!PyObject_TypeCheck(seq[i].ptr(),&DocumentObjectPy::Type)) {
                PyErr_SetString(PyExc_TypeError, "Expect element in sequence to be of type document object");
                return nullptr;
            }
            objs.push_back(static_cast<DocumentObjectPy*>(seq[i].ptr())->getDocumentObjectPtr());
        }
    }
    else {
        Base::PyTypeCheck(&obj, &DocumentObjectPy::Type,
            "Expect first argument to be either a document object, sequence of document objects or None");
        if (obj)
            objs.push_back(static_cast<DocumentObjectPy*>(obj)->getDocumentObjectPtr());
    }

    if (objs.empty())
        objs = getDocumentPtr()->getObjects();

    PY_TRY {
        auto ret = getDocumentPtr()->importLinks(objs);

        Py::Tuple tuple(ret.size());
        for (size_t i=0;i<ret.size();++i)
            tuple.setItem(i,Py::Object(ret[i]->getPyObject(),true));
        return Py::new_reference_to(tuple);
    }
    PY_CATCH
}

PyObject*  DocumentPy::moveObject(PyObject *args)
{
    PyObject *obj, *rec=Py_False;
    if (!PyArg_ParseTuple(args, "O!|O!",&(DocumentObjectPy::Type),&obj,&PyBool_Type,&rec))
        return nullptr;

    DocumentObjectPy* docObj = static_cast<DocumentObjectPy*>(obj);
    DocumentObject* move = getDocumentPtr()->moveObject(docObj->getDocumentObjectPtr(), Base::asBoolean(rec));
    if (move) {
        return move->getPyObject();
    }
    else {
        std::string str("Failed to move the object");
        throw Py::ValueError(str);
    }
}

PyObject*  DocumentPy::openTransaction(PyObject *args)
{
    PyObject *value = nullptr;
    if (!PyArg_ParseTuple(args, "|O",&value))
        return nullptr;
    std::string cmd;


    if (!value) {
        cmd = "<empty>";
    }
    else if (PyUnicode_Check(value)) {
        cmd = PyUnicode_AsUTF8(value);
    }
    else {
        PyErr_SetString(PyExc_TypeError, "string or unicode expected");
        return nullptr;
    }

    getDocumentPtr()->openTransaction(cmd.c_str());
    Py_Return;
}

PyObject*  DocumentPy::abortTransaction(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    getDocumentPtr()->abortTransaction();
    Py_Return;
}

PyObject*  DocumentPy::commitTransaction(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    getDocumentPtr()->commitTransaction();
    Py_Return;
}

Py::Boolean DocumentPy::getHasPendingTransaction() const {
    return {getDocumentPtr()->hasPendingTransaction()};
}

PyObject*  DocumentPy::undo(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    if (getDocumentPtr()->getAvailableUndos())
        getDocumentPtr()->undo();
    Py_Return;
}

PyObject*  DocumentPy::redo(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    if (getDocumentPtr()->getAvailableRedos())
        getDocumentPtr()->redo();
    Py_Return;
}

PyObject* DocumentPy::clearUndos(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    getDocumentPtr()->clearUndos();
    Py_Return;
}

PyObject* DocumentPy::clearDocument(PyObject * args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    getDocumentPtr()->clearDocument();
    Py_Return;
}

PyObject* DocumentPy::setClosable(PyObject* args)
{
    PyObject* close;
    if (!PyArg_ParseTuple(args, "O!", &PyBool_Type, &close))
        return nullptr;
    getDocumentPtr()->setClosable(Base::asBoolean(close));
    Py_Return;
}

PyObject* DocumentPy::isClosable(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    bool ok = getDocumentPtr()->isClosable();
    return Py::new_reference_to(Py::Boolean(ok));
}

PyObject*  DocumentPy::recompute(PyObject * args)
{
    PyObject *pyobjs = Py_None;
    PyObject *force = Py_False;
    PyObject *checkCycle = Py_False;
    if (!PyArg_ParseTuple(args, "|OO!O!",&pyobjs,
                &PyBool_Type,&force,&PyBool_Type,&checkCycle))
        return nullptr;

    PY_TRY {
        std::vector<App::DocumentObject *> objs;
        if (pyobjs!=Py_None) {
            if (!PySequence_Check(pyobjs)) {
                PyErr_SetString(PyExc_TypeError, "expect input of sequence of document objects");
                return nullptr;
            }

            Py::Sequence seq(pyobjs);
            for (Py_ssize_t i=0;i<seq.size();++i) {
                if (!PyObject_TypeCheck(seq[i].ptr(), &DocumentObjectPy::Type)) {
                    PyErr_SetString(PyExc_TypeError, "Expect element in sequence to be of type document object");
                    return nullptr;
                }
                objs.push_back(static_cast<DocumentObjectPy*>(seq[i].ptr())->getDocumentObjectPtr());
            }
        }

        int options = 0;
        if (Base::asBoolean(checkCycle))
            options = Document::DepNoCycle;

        int objectCount = getDocumentPtr()->recompute(objs, Base::asBoolean(force), nullptr, options);

        // Document::recompute() hides possibly raised Python exceptions by its features
        // So, check if an error is set and return null if yes
        if (PyErr_Occurred()) {
            return nullptr;
        }

        return Py::new_reference_to(Py::Int(objectCount));
    } PY_CATCH;
}

PyObject* DocumentPy::mustExecute(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    bool ok = getDocumentPtr()->mustExecute();
    return Py::new_reference_to(Py::Boolean(ok));
}

PyObject* DocumentPy::isTouched(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    bool ok = getDocumentPtr()->isTouched();
    return Py::new_reference_to(Py::Boolean(ok));
}

PyObject* DocumentPy::purgeTouched(PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;
    getDocumentPtr()->purgeTouched();
    Py_Return;
}

PyObject* DocumentPy::getObject(PyObject *args)
{
    DocumentObject* obj = nullptr;

    do {
        char* name = nullptr;
        if (PyArg_ParseTuple(args, "s", &name))  {
            obj = getDocumentPtr()->getObject(name);
            break;
        }

        PyErr_Clear();
        long id = -1;
        if (PyArg_ParseTuple(args, "l", &id)) {
            obj = getDocumentPtr()->getObjectByID(id);
            break;
        }

        PyErr_SetString(PyExc_TypeError, "a string or integer is required");
        return nullptr;
    }
    while (false);

    if (obj)
        return obj->getPyObject();

    Py_Return;
}

PyObject*  DocumentPy::getObjectsByLabel(PyObject *args)
{
    char *sName;
    if (!PyArg_ParseTuple(args, "s",&sName))
        return nullptr;

    Py::List list;
    std::string name = sName;
    std::vector<DocumentObject*> objs = getDocumentPtr()->getObjects();
    for (auto obj : objs) {
        if (name == obj->Label.getValue())
            list.append(Py::asObject(obj->getPyObject()));
    }

    return Py::new_reference_to(list);
}

PyObject*  DocumentPy::findObjects(PyObject *args, PyObject *kwds)
{
    const char *sType = "App::DocumentObject", *sName = nullptr, *sLabel = nullptr;
    static const std::array<const char *, 4> kwlist{"Type", "Name", "Label", nullptr};
    if (!Base::Wrapped_ParseTupleAndKeywords(args, kwds, "|sss", kwlist, &sType, &sName, &sLabel)) {
        return nullptr;
    }

    Base::Type type = Base::Type::getTypeIfDerivedFrom(sType, App::DocumentObject::getClassTypeId(), true);
    if (type.isBad()) {
        std::stringstream str;
        str << "'" << sType << "' is not a document object type";
        throw Base::TypeError(str.str());
    }

    std::vector<DocumentObject*> res;

    try {
        res = getDocumentPtr()->findObjects(type, sName, sLabel);
    }
    catch (const boost::regex_error& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }

    Py_ssize_t index=0;
    PyObject* list = PyList_New((Py_ssize_t)res.size());
    for (std::vector<DocumentObject*>::const_iterator It = res.begin();It != res.end();++It, index++)
        PyList_SetItem(list, index, (*It)->getPyObject());
    return list;
}

Py::Object DocumentPy::getActiveObject() const
{
    DocumentObject *pcFtr = getDocumentPtr()->getActiveObject();
    if (pcFtr)
        return Py::Object(pcFtr->getPyObject(), true);
    return Py::None();
}

PyObject*  DocumentPy::supportedTypes(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    std::vector<Base::Type> ary;
    Base::Type::getAllDerivedFrom(App::DocumentObject::getClassTypeId(), ary);
    Py::List res;
    for (const auto & it : ary)
        res.append(Py::String(it.getName()));
    return Py::new_reference_to(res);
}

Py::List DocumentPy::getObjects() const
{
    std::vector<DocumentObject*> objs = getDocumentPtr()->getObjects();
    Py::List res;

    for (auto obj : objs)
        //Note: Here we must force the Py::Object to own this Python object as getPyObject() increments the counter
        res.append(Py::Object(obj->getPyObject(), true));

    return res;
}

Py::List DocumentPy::getTopologicalSortedObjects() const
{
    std::vector<DocumentObject*> objs = getDocumentPtr()->topologicalSort();
    Py::List res;

    for (auto obj : objs)
        //Note: Here we must force the Py::Object to own this Python object as getPyObject() increments the counter
        res.append(Py::Object(obj->getPyObject(), true));

    return res;
}

Py::List DocumentPy::getRootObjects() const
{
    std::vector<DocumentObject*> objs = getDocumentPtr()->getRootObjects();
    Py::List res;

    for (auto obj : objs)
        //Note: Here we must force the Py::Object to own this Python object as getPyObject() increments the counter
        res.append(Py::Object(obj->getPyObject(), true));

    return res;
}

Py::Int DocumentPy::getUndoMode() const
{
    return Py::Int(getDocumentPtr()->getUndoMode());
}

void  DocumentPy::setUndoMode(Py::Int arg)
{
    getDocumentPtr()->setUndoMode(arg);
}


Py::Int DocumentPy::getUndoRedoMemSize() const
{
    return Py::Int((long)getDocumentPtr()->getUndoMemSize());
}

Py::Int DocumentPy::getUndoCount() const
{
    return Py::Int((long)getDocumentPtr()->getAvailableUndos());
}

Py::Int DocumentPy::getRedoCount() const
{
    return Py::Int((long)getDocumentPtr()->getAvailableRedos());
}

Py::List DocumentPy::getUndoNames() const
{
    std::vector<std::string> vList = getDocumentPtr()->getAvailableUndoNames();
    Py::List res;

    for (const auto & It : vList)
        res.append(Py::String(It));

    return res;
}

Py::List DocumentPy::getRedoNames() const
{
    std::vector<std::string> vList = getDocumentPtr()->getAvailableRedoNames();
    Py::List res;

    for (const auto & It : vList)
        res.append(Py::String(It));

    return res;
}

Py::String  DocumentPy::getDependencyGraph() const
{
    std::stringstream out;
    getDocumentPtr()->exportGraphviz(out);
    return {out.str()};
}

Py::String DocumentPy::getName() const
{
    return {getDocumentPtr()->getName()};
}

Py::Boolean DocumentPy::getRecomputesFrozen() const
{
    return {getDocumentPtr()->testStatus(Document::Status::SkipRecompute)};
}

void DocumentPy::setRecomputesFrozen(Py::Boolean arg)
{
    getDocumentPtr()->setStatus(Document::Status::SkipRecompute, arg.isTrue());
}

PyObject* DocumentPy::getTempFileName(PyObject *args)
{
    PyObject *value;
    if (!PyArg_ParseTuple(args, "O",&value))
        return nullptr;

    std::string string;
    if (PyUnicode_Check(value)) {
        string = PyUnicode_AsUTF8(value);
    }
    else {
        std::string error = std::string("type must be a string!");
        error += value->ob_type->tp_name;
        throw Py::TypeError(error);
    }

    // search for a temp file name in the document transient directory
    Base::FileInfo fileName(Base::FileInfo::getTempFileName
        (string.c_str(),getDocumentPtr()->TransientDir.getValue()));
    // delete the created file, we need only the name...
    fileName.deleteFile();

    PyObject *p = PyUnicode_DecodeUTF8(fileName.filePath().c_str(),fileName.filePath().size(),nullptr);
    if (!p) {
        throw Base::UnicodeError("UTF8 conversion failure at PropertyString::getPyObject()");
    }
    return p;
}

PyObject *DocumentPy::getCustomAttributes(const char* attr) const
{
    // Note: Here we want to return only a document object if its
    // name matches 'attr'. However, it is possible to have an object
    // with the same name as an attribute. If so, we return 0 as other-
    // wise it wouldn't be possible to address this attribute any more.
    // The object must then be addressed by the getObject() method directly.
    App::Property* prop = getPropertyContainerPtr()->getPropertyByName(attr);
    if (prop)
        return nullptr;
    if (!this->ob_type->tp_dict) {
        if (PyType_Ready(this->ob_type) < 0)
            return nullptr;
    }
    PyObject* item = PyDict_GetItemString(this->ob_type->tp_dict, attr);
    if (item)
        return nullptr;
    // search for an object with this name
    DocumentObject* obj = getDocumentPtr()->getObject(attr);
    return (obj ? obj->getPyObject() : nullptr);
}

int DocumentPy::setCustomAttributes(const char* attr, PyObject *)
{
    // Note: Here we want to return only a document object if its
    // name matches 'attr'. However, it is possible to have an object
    // with the same name as an attribute. If so, we return 0 as other-
    // wise it wouldn't be possible to address this attribute any more.
    // The object must then be addressed by the getObject() method directly.
    App::Property* prop = getPropertyContainerPtr()->getPropertyByName(attr);
    if (prop)
        return 0;
    if (!this->ob_type->tp_dict) {
        if (PyType_Ready(this->ob_type) < 0)
            return 0;
    }
    PyObject* item = PyDict_GetItemString(this->ob_type->tp_dict, attr);
    if (item)
        return 0;
    DocumentObject* obj = getDocumentPtr()->getObject(attr);
    if (obj)
    {
        std::stringstream str;
        str << "'Document' object attribute '" << attr
            << "' must not be set this way" << std::ends;
        PyErr_SetString(PyExc_RuntimeError, str.str().c_str());
        return -1;
    }

    return 0;
}

PyObject* DocumentPy::getLinksTo(PyObject *args)
{
    PyObject *pyobj = Py_None;
    int options = 0;
    short count = 0;
    if (!PyArg_ParseTuple(args, "|Oih", &pyobj,&options, &count))
        return nullptr;

    PY_TRY {
        Base::PyTypeCheck(&pyobj, &DocumentObjectPy::Type, "Expect the first argument of type document object");
        DocumentObject *obj = nullptr;
        if (pyobj)
            obj = static_cast<DocumentObjectPy*>(pyobj)->getDocumentObjectPtr();

        std::set<DocumentObject *> links;
        getDocumentPtr()->getLinksTo(links,obj,options,count);
        Py::Tuple ret(links.size());
        int i=0;
        for (auto o : links)
            ret.setItem(i++,Py::Object(o->getPyObject(),true));

        return Py::new_reference_to(ret);
    }
    PY_CATCH
}

Py::List DocumentPy::getInList() const
{
    Py::List ret;
    auto lists = PropertyXLink::getDocumentInList(getDocumentPtr());
    if (lists.size()==1) {
        for (auto doc : lists.begin()->second)
            ret.append(Py::Object(doc->getPyObject(), true));
    }
    return ret;
}

Py::List DocumentPy::getOutList() const
{
    Py::List ret;
    auto lists = PropertyXLink::getDocumentOutList(getDocumentPtr());
    if (lists.size()==1) {
        for (auto doc : lists.begin()->second)
            ret.append(Py::Object(doc->getPyObject(), true));
    }
    return ret;
}

PyObject *DocumentPy::getDependentDocuments(PyObject *args) {
    PyObject *sort = Py_True;
    if (!PyArg_ParseTuple(args, "|O!", &PyBool_Type, &sort))
        return nullptr;
    PY_TRY {
        auto docs = getDocumentPtr()->getDependentDocuments(Base::asBoolean(sort));
        Py::List ret;
        for (auto doc : docs)
            ret.append(Py::Object(doc->getPyObject(), true));
        return Py::new_reference_to(ret);
    } PY_CATCH;
}

Py::Boolean DocumentPy::getRestoring() const
{
    return {getDocumentPtr()->testStatus(Document::Status::Restoring)};
}

Py::Boolean DocumentPy::getPartial() const
{
    return {getDocumentPtr()->testStatus(Document::Status::PartialDoc)};
}

Py::Boolean DocumentPy::getImporting() const
{
    return {getDocumentPtr()->testStatus(Document::Status::Importing)};
}

Py::Boolean DocumentPy::getRecomputing() const
{
    return {getDocumentPtr()->testStatus(Document::Status::Recomputing)};
}

Py::Boolean DocumentPy::getTransacting() const
{
    return {getDocumentPtr()->isPerformingTransaction()};
}

Py::String DocumentPy::getOldLabel() const
{
    return {getDocumentPtr()->getOldLabel()};
}

Py::Boolean DocumentPy::getTemporary() const
{
    return {getDocumentPtr()->testStatus(Document::TempDoc)};
}
