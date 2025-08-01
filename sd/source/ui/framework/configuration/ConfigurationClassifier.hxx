/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#pragma once

#include "debugtrace.hxx"
#include <com/sun/star/uno/Reference.hxx>
#include <rtl/ref.hxx>
#include <vector>

namespace sd::framework
{
class Configuration;
}

namespace sd::framework
{
class ResourceId;

/** A ConfigurationClassifier object compares two configurations of
    resources and gives access to the differences.  It is used mainly when
    changes to the current configuration have been requested and the various
    resource controllers have to be supplied with the set of resources that
    are to be activated or deactivated.
*/
class ConfigurationClassifier
{
public:
    /** Create a new ConfigurationClassifier object that will compare the
        two given configurations.
    */
    ConfigurationClassifier(const rtl::Reference<sd::framework::Configuration>& rxConfiguration1,
                            const rtl::Reference<sd::framework::Configuration>& rxConfiguration2);

    /** Calculate three lists of resource ids.  These contain the resources
        that belong to one configuration but not the other, or that belong
        to both configurations.
        @return
            When the two configurations differ then return <TRUE/>.  When
            they are equivalent then return <FALSE/>.
    */
    bool Partition();

    typedef ::std::vector<rtl::Reference<sd::framework::ResourceId>> ResourceIdVector;

    /** Return the resources that belong to the configuration given as
        rxConfiguration1 to the constructor but that do not belong to
        rxConfiguration2.
        @return
            A reference to the, possibly empty, list of resources is
            returned.  This reference remains valid as long as the called
            ConfigurationClassifier object stays alive.
    */
    const ResourceIdVector& GetC1minusC2() const { return maC1minusC2; }

    /** Return the resources that belong to the configuration given as
        rxConfiguration2 to the constructor but that do not belong to
        rxConfiguration1.
        @return
            A reference to the, possibly empty, list of resources is
            returned.  This reference remains valid as long as the called
            ConfigurationClassifier object stays alive.
    */
    const ResourceIdVector& GetC2minusC1() const { return maC2minusC1; }

#if DEBUG_SD_CONFIGURATION_TRACE

    /** Return the resources that belong to both the configurations that
        where given to the constructor.
        @return
            A reference to the, possibly empty, list of resources is
            returned.  This reference remains valid as long as the called
            ConfigurationClassifier object stays alive.
    */
    const ResourceIdVector& GetC1andC2() const { return maC1andC2; }

    static void TraceResourceIdVector(const char* pMessage, const ResourceIdVector& rResources);

#endif

private:
    rtl::Reference<sd::framework::Configuration> mxConfiguration1;
    rtl::Reference<sd::framework::Configuration> mxConfiguration2;

    /** After the call to Classify() this vector holds all elements from
        mxConfiguration1 that are not in mxConfiguration2.
    */
    ResourceIdVector maC1minusC2;

    /** After the call to Classify() this vector holds all elements from
        mxConfiguration2 that are not in mxConfiguration1.
    */
    ResourceIdVector maC2minusC1;

    /** Put all the elements in the two given sequences of resource ids and
        copy them into one of the resource id result vectors maC1minusC2,
        maC2minusC1, and maC1andC2.  This is done by using only the resource
        URLs for classification.  Therefore this method calls itself
        recursively.
        @param rS1
            One sequence of ResourceId objects.
        @param rS2
            Another sequence of ResourceId objects.
    */
    void PartitionResources(const std::vector<rtl::Reference<sd::framework::ResourceId>>& rS1,
                            const std::vector<rtl::Reference<sd::framework::ResourceId>>& rS2);

    /** Compare the given sequences of resource ids and put their elements
        in one of three vectors depending on whether an element belongs to
        both sequences or to one but not the other.  Note that only the
        resource URLs of the ResourceId objects are used for the
        classification.
        @param rS1
            One sequence of ResourceId objects.
        @param rS2
            Another sequence of ResourceId objects.
    */
    static void ClassifyResources(const std::vector<rtl::Reference<sd::framework::ResourceId>>& rS1,
                                  const std::vector<rtl::Reference<sd::framework::ResourceId>>& rS2,
                                  ResourceIdVector& rS1minusS2, ResourceIdVector& rS2minusS1,
                                  ResourceIdVector& rS1andS2);

    /** Copy the resources given in rSource to the list of resources
        specified by rTarget.  Resources bound to the ones in rSource,
        either directly or indirectly, are copied as well.
        @param rSource
            All resources and the ones bound to them, either directly or
            indirectly, are copied.
        @param rxConfiguration
            This configuration is used to determine the resources bound to
            the ones in rSource.
        @param rTarget
            This list is filled with resources from rSource and the ones
            bound to them.
    */
    static void CopyResources(const ResourceIdVector& rSource,
                              const rtl::Reference<sd::framework::Configuration>& rxConfiguration,
                              ResourceIdVector& rTarget);
};

} // end of namespace sd::framework

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
