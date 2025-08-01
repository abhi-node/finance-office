/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <oox/token/tokens.hxx>
#include <drawingml/table/tablestyle.hxx>
#include <frozen/bits/defines.h>
#include <frozen/bits/elsa_std.h>
#include <frozen/unordered_map.h>
#include <unordered_map>

using namespace oox;
using namespace oox::drawingml::table;

/* tdf#107604
 * There are predefined table styles that have a
 * style id (in ppt/slides/slidex.xml) but does not have
 * corresponding style definition (ppt/tableStyles.xml).
 * So we should create those styles here for this case.
 * There are 74 predefined styles and many different
 * variables. A style map was created by examining all
 * 74 style properties. And table styles were coded according
 * to that map. You can see that map in
 * oox/documentation/predefined-styles-map.ods. We should
 * define all of these variables to keep the code readable
 * and change something easily when some styles change.
 */

// Create style-id map for using similar attributes of the groups.
// (style ids used from here: https://docs.microsoft.com/en-us/previous-versions/office/developer/office-2010/hh273476(v=office.14)?redirectedfrom=MSDN)
// and checked all of them.

const std::unordered_map<OUString, std::pair<OUString, OUString>> mStyleIdMap{
    { u"{2D5ABB26-0587-4C30-8999-92F81FD0307C}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u""_ustr) },
    { u"{3C2FFA5D-87B4-456A-9821-1D502468CF0F}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u"Accent1"_ustr) },
    { u"{284E427A-3D55-4303-BF80-6455036E1DE7}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u"Accent2"_ustr) },
    { u"{69C7853C-536D-4A76-A0AE-DD22124D55A5}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u"Accent3"_ustr) },
    { u"{775DCB02-9BB8-47FD-8907-85C794F793BA}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u"Accent4"_ustr) },
    { u"{35758FB7-9AC5-4552-8A53-C91805E547FA}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u"Accent5"_ustr) },
    { u"{08FB837D-C827-4EFA-A057-4D05807E0F7C}"_ustr,
      std::make_pair(u"Themed-Style-1"_ustr, u"Accent6"_ustr) },

    { u"{5940675A-B579-460E-94D1-54222C63F5DA}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u""_ustr) },
    { u"{D113A9D2-9D6B-4929-AA2D-F23B5EE8CBE7}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u"Accent1"_ustr) },
    { u"{18603FDC-E32A-4AB5-989C-0864C3EAD2B8}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u"Accent2"_ustr) },
    { u"{306799F8-075E-4A3A-A7F6-7FBC6576F1A4}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u"Accent3"_ustr) },
    { u"{E269D01E-BC32-4049-B463-5C60D7B0CCD2}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u"Accent4"_ustr) },
    { u"{327F97BB-C833-4FB7-BDE5-3F7075034690}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u"Accent5"_ustr) },
    { u"{638B1855-1B75-4FBE-930C-398BA8C253C6}"_ustr,
      std::make_pair(u"Themed-Style-2"_ustr, u"Accent6"_ustr) },

    { u"{9D7B26C5-4107-4FEC-AEDC-1716B250A1EF}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u""_ustr) },
    { u"{3B4B98B0-60AC-42C2-AFA5-B58CD77FA1E5}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u"Accent1"_ustr) },
    { u"{0E3FDE45-AF77-4B5C-9715-49D594BDF05E}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u"Accent2"_ustr) },
    { u"{C083E6E3-FA7D-4D7B-A595-EF9225AFEA82}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u"Accent3"_ustr) },
    { u"{D27102A9-8310-4765-A935-A1911B00CA55}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u"Accent4"_ustr) },
    { u"{5FD0F851-EC5A-4D38-B0AD-8093EC10F338}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u"Accent5"_ustr) },
    { u"{68D230F3-CF80-4859-8CE7-A43EE81993B5}"_ustr,
      std::make_pair(u"Light-Style-1"_ustr, u"Accent6"_ustr) },

    { u"{7E9639D4-E3E2-4D34-9284-5A2195B3D0D7}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u""_ustr) },
    { u"{69012ECD-51FC-41F1-AA8D-1B2483CD663E}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u"Accent1"_ustr) },
    { u"{72833802-FEF1-4C79-8D5D-14CF1EAF98D9}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u"Accent2"_ustr) },
    { u"{F2DE63D5-997A-4646-A377-4702673A728D}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u"Accent3"_ustr) },
    { u"{17292A2E-F333-43FB-9621-5CBBE7FDCDCB}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u"Accent4"_ustr) },
    { u"{5A111915-BE36-4E01-A7E5-04B1672EAD32}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u"Accent5"_ustr) },
    { u"{912C8C85-51F0-491E-9774-3900AFEF0FD7}"_ustr,
      std::make_pair(u"Light-Style-2"_ustr, u"Accent6"_ustr) },

    { u"{616DA210-FB5B-4158-B5E0-FEB733F419BA}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u""_ustr) },
    { u"{BC89EF96-8CEA-46FF-86C4-4CE0E7609802}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u"Accent1"_ustr) },
    { u"{5DA37D80-6434-44D0-A028-1B22A696006F}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u"Accent2"_ustr) },
    { u"{8799B23B-EC83-4686-B30A-512413B5E67A}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u"Accent3"_ustr) },
    { u"{ED083AE6-46FA-4A59-8FB0-9F97EB10719F}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u"Accent4"_ustr) },
    { u"{BDBED569-4797-4DF1-A0F4-6AAB3CD982D8}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u"Accent5"_ustr) },
    { u"{E8B1032C-EA38-4F05-BA0D-38AFFFC7BED3}"_ustr,
      std::make_pair(u"Light-Style-3"_ustr, u"Accent6"_ustr) },

    { u"{793D81CF-94F2-401A-BA57-92F5A7B2D0C5}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u""_ustr) },
    { u"{B301B821-A1FF-4177-AEE7-76D212191A09}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u"Accent1"_ustr) },
    { u"{9DCAF9ED-07DC-4A11-8D7F-57B35C25682E}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u"Accent2"_ustr) },
    { u"{1FECB4D8-DB02-4DC6-A0A2-4F2EBAE1DC90}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u"Accent3"_ustr) },
    { u"{1E171933-4619-4E11-9A3F-F7608DF75F80}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u"Accent4"_ustr) },
    { u"{FABFCF23-3B69-468F-B69F-88F6DE6A72F2}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u"Accent5"_ustr) },
    { u"{10A1B5D5-9B99-4C35-A422-299274C87663}"_ustr,
      std::make_pair(u"Medium-Style-1"_ustr, u"Accent6"_ustr) },

    { u"{073A0DAA-6AF3-43AB-8588-CEC1D06C72B9}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u""_ustr) },
    { u"{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u"Accent1"_ustr) },
    { u"{21E4AEA4-8DFA-4A89-87EB-49C32662AFE0}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u"Accent2"_ustr) },
    { u"{F5AB1C69-6EDB-4FF4-983F-18BD219EF322}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u"Accent3"_ustr) },
    { u"{00A15C55-8517-42AA-B614-E9B94910E393}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u"Accent4"_ustr) },
    { u"{7DF18680-E054-41AD-8BC1-D1AEF772440D}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u"Accent5"_ustr) },
    { u"{93296810-A885-4BE3-A3E7-6D5BEEA58F35}"_ustr,
      std::make_pair(u"Medium-Style-2"_ustr, u"Accent6"_ustr) },

    { u"{8EC20E35-A176-4012-BC5E-935CFFF8708E}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u""_ustr) },
    { u"{6E25E649-3F16-4E02-A733-19D2CDBF48F0}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u"Accent1"_ustr) },
    { u"{85BE263C-DBD7-4A20-BB59-AAB30ACAA65A}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u"Accent2"_ustr) },
    { u"{EB344D84-9AFB-497E-A393-DC336BA19D2E}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u"Accent3"_ustr) },
    { u"{EB9631B5-78F2-41C9-869B-9F39066F8104}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u"Accent4"_ustr) },
    { u"{74C1A8A3-306A-4EB7-A6B1-4F7E0EB9C5D6}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u"Accent5"_ustr) },
    { u"{2A488322-F2BA-4B5B-9748-0D474271808F}"_ustr,
      std::make_pair(u"Medium-Style-3"_ustr, u"Accent6"_ustr) },

    { u"{D7AC3CCA-C797-4891-BE02-D94E43425B78}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u""_ustr) },
    { u"{69CF1AB2-1976-4502-BF36-3FF5EA218861}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u"Accent1"_ustr) },
    { u"{8A107856-5554-42FB-B03E-39F5DBC370BA}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u"Accent2"_ustr) },
    { u"{0505E3EF-67EA-436B-97B2-0124C06EBD24}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u"Accent3"_ustr) },
    { u"{C4B1156A-380E-4F78-BDF5-A606A8083BF9}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u"Accent4"_ustr) },
    { u"{22838BEF-8BB2-4498-84A7-C5851F593DF1}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u"Accent5"_ustr) },
    { u"{16D9F66E-5EB9-4882-86FB-DCBF35E3C3E4}"_ustr,
      std::make_pair(u"Medium-Style-4"_ustr, u"Accent6"_ustr) },

    { u"{E8034E78-7F5D-4C2E-B375-FC64B27BC917}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u""_ustr) },
    { u"{125E5076-3810-47DD-B79F-674D7AD40C01}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u"Accent1"_ustr) },
    { u"{37CE84F3-28C3-443E-9E96-99CF82512B78}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u"Accent2"_ustr) },
    { u"{D03447BB-5D67-496B-8E87-E561075AD55C}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u"Accent3"_ustr) },
    { u"{E929F9F4-4A8F-4326-A1B4-22849713DDAB}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u"Accent4"_ustr) },
    { u"{8FD4443E-F989-4FC4-A0C8-D5A2AF1F390B}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u"Accent5"_ustr) },
    { u"{AF606853-7671-496A-8E4F-DF71F8EC918B}"_ustr,
      std::make_pair(u"Dark-Style-1"_ustr, u"Accent6"_ustr) },

    { u"{5202B0CA-FC54-4496-8BCA-5EF66A818D29}"_ustr,
      std::make_pair(u"Dark-Style-2"_ustr, u""_ustr) },
    { u"{0660B408-B3CF-4A94-85FC-2B1E0A45F4A2}"_ustr,
      std::make_pair(u"Dark-Style-2"_ustr, u"Accent1"_ustr) },
    { u"{91EBBBCC-DAD2-459C-BE2E-F6DE35CF9A28}"_ustr,
      std::make_pair(u"Dark-Style-2"_ustr, u"Accent3"_ustr) },
    { u"{46F890A9-2807-4EBB-B81D-B2AA78EC7F39}"_ustr,
      std::make_pair(u"Dark-Style-2"_ustr, u"Accent5"_ustr) }
};

constexpr auto tokens
    = frozen::make_unordered_map<std::u16string_view, sal_Int32>({ { u"Accent1", XML_accent1 },
                                                                   { u"Accent2", XML_accent2 },
                                                                   { u"Accent3", XML_accent3 },
                                                                   { u"Accent4", XML_accent4 },
                                                                   { u"Accent5", XML_accent5 },
                                                                   { u"Accent6", XML_accent6 } });

sal_Int32 resolveToken(OUString const& rString)
{
    auto iterator = tokens.find(rString);
    if (iterator != tokens.end())
        return iterator->second;
    return XML_dk1;
}

void setBorderLineType(const oox::drawingml::LinePropertiesPtr& pLineProp, sal_Int32 nToken)
{
    pLineProp->maLineFill.moFillType = nToken;
}

void insertBorderLine(TableStylePart& aTableStylePart, sal_Int32 nToken,
                      const oox::drawingml::LinePropertiesPtr& pLineProp)
{
    if (pLineProp->maLineFill.moFillType.has_value())
    {
        aTableStylePart.getLineBorders().insert(
            std::pair<sal_Int32, ::oox::drawingml::LinePropertiesPtr>(nToken, pLineProp));
    }
}

std::unique_ptr<TableStyle> CreateTableStyle(const OUString& styleId)
{
    std::unique_ptr<TableStyle> pTableStyle;
    pTableStyle.reset(new TableStyle());

    // Text Style definitions for table parts

    bool bFirstRowTextBoldStyle = false;
    bool bFirstColTextBoldStyle = false;
    bool bLastColTextBoldStyle = false;

    // Text Color definitions for table parts

    ::oox::drawingml::Color wholeTblTextColor;
    ::oox::drawingml::Color firstRowTextColor;
    ::oox::drawingml::Color firstColTextColor;
    ::oox::drawingml::Color lastRowTextColor;
    ::oox::drawingml::Color lastColTextColor;

    // Fill properties definitions for table parts

    oox::drawingml::FillPropertiesPtr pWholeTblFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pFirstRowFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pFirstColFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pLastRowFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pLastColFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pBand1HFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pBand1VFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pBand2HFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pBand2VFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();
    oox::drawingml::FillPropertiesPtr pTblBgFillProperties
        = std::make_shared<oox::drawingml::FillProperties>();

    // Start table border line properties definitions for table parts

    oox::drawingml::LinePropertiesPtr pWholeTblLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pWholeTblRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pWholeTblTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pWholeTblBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pWholeTblInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pWholeTblInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pFirstRowLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstRowRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstRowTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstRowBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstRowInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstRowInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pFirstColLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstColRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstColTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstColBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstColInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pFirstColInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pLastColLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastColRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastColTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastColBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastColInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastColInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pLastRowLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastRowRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastRowTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastRowBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastRowInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pLastRowInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pBand1HLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1HRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1HTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1HBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1HInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1HInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pBand1VLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1VRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1VTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1VBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1VInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand1VInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pBand2HLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2HRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2HTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2HBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2HInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2HInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    oox::drawingml::LinePropertiesPtr pBand2VLeftBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2VRightBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2VTopBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2VBottomBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2VInsideHBorder
        = std::make_shared<oox::drawingml::LineProperties>();
    oox::drawingml::LinePropertiesPtr pBand2VInsideVBorder
        = std::make_shared<oox::drawingml::LineProperties>();

    // End table border line properties definitions for table parts

    // Start to set fill types.

    pTblBgFillProperties->moFillType = XML_solidFill;
    pWholeTblFillProperties->moFillType = XML_solidFill;
    pFirstRowFillProperties->moFillType = XML_solidFill;
    pFirstColFillProperties->moFillType = XML_solidFill;
    pLastRowFillProperties->moFillType = XML_solidFill;
    pLastColFillProperties->moFillType = XML_solidFill;
    pBand1HFillProperties->moFillType = XML_solidFill;
    pBand1VFillProperties->moFillType = XML_solidFill;
    pBand2HFillProperties->moFillType = XML_solidFill;
    pBand2VFillProperties->moFillType = XML_solidFill;

    // End to set fill types.

    // Define common properties.

    pWholeTblLeftBorder->moLineWidth = 12700;
    pWholeTblRightBorder->moLineWidth = 12700;
    pWholeTblTopBorder->moLineWidth = 12700;
    pWholeTblBottomBorder->moLineWidth = 12700;
    pWholeTblInsideHBorder->moLineWidth = 12700;
    pWholeTblInsideVBorder->moLineWidth = 12700;
    pFirstRowBottomBorder->moLineWidth = 12700;

    pWholeTblLeftBorder->moPresetDash = XML_solid;
    pWholeTblRightBorder->moPresetDash = XML_solid;
    pWholeTblTopBorder->moPresetDash = XML_solid;
    pWholeTblBottomBorder->moPresetDash = XML_solid;
    pWholeTblInsideHBorder->moPresetDash = XML_solid;
    pWholeTblInsideVBorder->moPresetDash = XML_solid;
    pFirstRowBottomBorder->moPresetDash = XML_solid;

    // Start to handle all style groups.

    auto it = mStyleIdMap.find(styleId);
    OUString style_name = it->second.first;
    OUString accent_name = it->second.second;

    if (style_name == "Themed-Style-1")
    {
        if (!accent_name.isEmpty())
        {
            setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
            setBorderLineType(pWholeTblRightBorder, XML_solidFill);
            setBorderLineType(pWholeTblTopBorder, XML_solidFill);
            setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
            setBorderLineType(pWholeTblInsideHBorder, XML_solidFill);
            setBorderLineType(pWholeTblInsideVBorder, XML_solidFill);
            setBorderLineType(pFirstRowLeftBorder, XML_solidFill);
            setBorderLineType(pFirstRowRightBorder, XML_solidFill);
            setBorderLineType(pFirstRowTopBorder, XML_solidFill);
            setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
            setBorderLineType(pLastRowLeftBorder, XML_solidFill);
            setBorderLineType(pLastRowRightBorder, XML_solidFill);
            setBorderLineType(pLastRowTopBorder, XML_solidFill);
            setBorderLineType(pLastRowBottomBorder, XML_solidFill);
            setBorderLineType(pFirstColLeftBorder, XML_solidFill);
            setBorderLineType(pFirstColRightBorder, XML_solidFill);
            setBorderLineType(pFirstColTopBorder, XML_solidFill);
            setBorderLineType(pFirstColBottomBorder, XML_solidFill);
            setBorderLineType(pFirstColInsideHBorder, XML_solidFill);
            setBorderLineType(pLastColLeftBorder, XML_solidFill);
            setBorderLineType(pLastColRightBorder, XML_solidFill);
            setBorderLineType(pLastColTopBorder, XML_solidFill);
            setBorderLineType(pLastColBottomBorder, XML_solidFill);
            setBorderLineType(pLastColInsideHBorder, XML_solidFill);

            sal_Int32 accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);

            wholeTblTextColor.setSchemeClr(XML_dk1);
            firstRowTextColor.setSchemeClr(XML_lt1);

            pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pWholeTblInsideHBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pWholeTblInsideVBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

            pFirstRowLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstRowRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstRowTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
            pFirstRowFillProperties->maFillColor.setSchemeClr(accent_val);

            pLastRowLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastRowRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastRowBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

            pFirstColLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstColRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstColTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstColBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pFirstColInsideHBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

            pLastColLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastColRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastColTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastColBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
            pLastColInsideHBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

            pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
            pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);
        }
        else
        {
            wholeTblTextColor.setSchemeClr(XML_tx1);
        }

        pBand1HFillProperties->maFillColor.addTransformation(XML_alpha, 40000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_alpha, 40000);
    }
    else if (style_name == "Themed-Style-2")
    {
        setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
        setBorderLineType(pWholeTblRightBorder, XML_solidFill);
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
        {
            setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
            setBorderLineType(pLastRowTopBorder, XML_solidFill);
            setBorderLineType(pFirstColRightBorder, XML_solidFill);
            setBorderLineType(pLastColLeftBorder, XML_solidFill);

            wholeTblTextColor.setSchemeClr(XML_lt1);
            firstRowTextColor.setSchemeClr(XML_lt1);

            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);

            pTblBgFillProperties->maFillColor.setSchemeClr(accent_val);
            pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
            pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
            pFirstColRightBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
            pLastColLeftBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
            pBand1HFillProperties->maFillColor.setSchemeClr(XML_lt1);
            pBand1VFillProperties->maFillColor.setSchemeClr(XML_lt1);
        }
        else
        {
            setBorderLineType(pWholeTblInsideVBorder, XML_solidFill);
            setBorderLineType(pWholeTblInsideHBorder, XML_solidFill);

            accent_val = XML_tx1;

            pWholeTblInsideVBorder->maLineFill.maFillColor.setSchemeClr(XML_tx1);
            pWholeTblInsideHBorder->maLineFill.maFillColor.setSchemeClr(XML_tx1);
        }

        pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pBand1HFillProperties->maFillColor.addTransformation(XML_alpha, 20000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_alpha, 20000);
        pWholeTblLeftBorder->maLineFill.maFillColor.addTransformation(XML_tint, 50000);
        pWholeTblRightBorder->maLineFill.maFillColor.addTransformation(XML_tint, 50000);
        pWholeTblTopBorder->maLineFill.maFillColor.addTransformation(XML_tint, 50000);
        pWholeTblBottomBorder->maLineFill.maFillColor.addTransformation(XML_tint, 50000);
    }
    else if (style_name == "Light-Style-1")
    {
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);

        bFirstRowTextBoldStyle = true;
        bFirstColTextBoldStyle = true;
        bLastColTextBoldStyle = true;

        wholeTblTextColor.setSchemeClr(XML_tx1);
        firstRowTextColor.setSchemeClr(XML_tx1);
        lastColTextColor.setSchemeClr(XML_tx1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_tx1;

        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pBand1HFillProperties->maFillColor.addTransformation(XML_alpha, 20000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_alpha, 20000);
    }
    else if (style_name == "Light-Style-2")
    {
        setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
        setBorderLineType(pWholeTblRightBorder, XML_solidFill);
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);
        setBorderLineType(pBand1HTopBorder, XML_solidFill);
        setBorderLineType(pBand1HBottomBorder, XML_solidFill);
        setBorderLineType(pBand1VLeftBorder, XML_solidFill);
        setBorderLineType(pBand1VRightBorder, XML_solidFill);
        setBorderLineType(pBand2VLeftBorder, XML_solidFill);
        setBorderLineType(pBand2VRightBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_tx1);
        firstRowTextColor.setSchemeClr(XML_bg1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_tx1;

        pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pFirstRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pBand1HTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pBand1HBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pBand1VLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pBand1VRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pBand2VLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pBand2VRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
    }
    else if (style_name == "Light-Style-3")
    {
        setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
        setBorderLineType(pWholeTblRightBorder, XML_solidFill);
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideHBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideVBorder, XML_solidFill);
        setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_tx1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_tx1;

        pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblInsideHBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblInsideVBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        firstRowTextColor.setSchemeClr(accent_val);
        pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pBand1HFillProperties->maFillColor.addTransformation(XML_alpha, 20000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_alpha, 20000);
    }
    else if (style_name == "Medium-Style-1")
    {
        setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
        setBorderLineType(pWholeTblRightBorder, XML_solidFill);
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideHBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_dk1);
        firstRowTextColor.setSchemeClr(XML_lt1);
        pWholeTblFillProperties->maFillColor.setSchemeClr(XML_lt1);
        pLastRowFillProperties->maFillColor.setSchemeClr(XML_lt1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_dk1;

        pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblInsideHBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pFirstRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);

        pBand1HFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_tint, 20000);
    }
    else if (style_name == "Medium-Style-2")
    {
        setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
        setBorderLineType(pWholeTblRightBorder, XML_solidFill);
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideHBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideVBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_dk1);
        firstRowTextColor.setSchemeClr(XML_lt1);
        lastRowTextColor.setSchemeClr(XML_lt1);
        firstColTextColor.setSchemeClr(XML_lt1);
        lastColTextColor.setSchemeClr(XML_lt1);
        pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);

        pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pWholeTblInsideHBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pWholeTblInsideVBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_dk1;

        pWholeTblFillProperties->maFillColor.setSchemeClr(accent_val);
        pFirstRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pFirstColFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastColFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pWholeTblFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pBand1HFillProperties->maFillColor.addTransformation(XML_tint, 40000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_tint, 40000);
    }
    else if (style_name == "Medium-Style-3")
    {
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_dk1);
        firstColTextColor.setSchemeClr(XML_lt1);
        lastColTextColor.setSchemeClr(XML_lt1);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(XML_dk1);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_dk1);
        pWholeTblFillProperties->maFillColor.setSchemeClr(XML_lt1);
        pLastRowFillProperties->maFillColor.setSchemeClr(XML_lt1);
        pBand1HFillProperties->maFillColor.setSchemeClr(XML_dk1);
        pBand1VFillProperties->maFillColor.setSchemeClr(XML_dk1);

        firstRowTextColor.setSchemeClr(XML_lt1);
        pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_dk1);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(XML_dk1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_dk1;

        pFirstRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pFirstColFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastColFillProperties->maFillColor.setSchemeClr(accent_val);

        pBand1HFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_tint, 20000);
    }
    else if (style_name == "Medium-Style-4")
    {
        setBorderLineType(pWholeTblLeftBorder, XML_solidFill);
        setBorderLineType(pWholeTblRightBorder, XML_solidFill);
        setBorderLineType(pWholeTblTopBorder, XML_solidFill);
        setBorderLineType(pWholeTblBottomBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideHBorder, XML_solidFill);
        setBorderLineType(pWholeTblInsideVBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_dk1);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(XML_dk1);
        pLastRowFillProperties->maFillColor.setSchemeClr(XML_dk1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_dk1;

        pWholeTblLeftBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblRightBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblTopBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblBottomBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblInsideHBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblInsideVBorder->maLineFill.maFillColor.setSchemeClr(accent_val);
        pWholeTblFillProperties->maFillColor.setSchemeClr(accent_val);

        firstRowTextColor.setSchemeClr(accent_val);
        pFirstRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pFirstRowFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pLastRowFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pWholeTblFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pBand1HFillProperties->maFillColor.addTransformation(XML_tint, 40000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_tint, 40000);
    }
    else if (style_name == "Dark-Style-1")
    {
        setBorderLineType(pFirstRowBottomBorder, XML_solidFill);
        setBorderLineType(pFirstColRightBorder, XML_solidFill);
        setBorderLineType(pLastColLeftBorder, XML_solidFill);
        setBorderLineType(pLastRowTopBorder, XML_solidFill);

        sal_Int32 transform_val;
        wholeTblTextColor.setSchemeClr(XML_dk1);
        firstRowTextColor.setSchemeClr(XML_lt1);
        pFirstRowBottomBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pFirstColRightBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pLastColLeftBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);
        pFirstRowFillProperties->maFillColor.setSchemeClr(XML_dk1);
        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(XML_lt1);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
        {
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
            transform_val = XML_shade;
        }
        else
        {
            accent_val = XML_dk1;
            transform_val = XML_tint;
        }

        pWholeTblFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pFirstColFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastColFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pWholeTblFillProperties->maFillColor.addTransformation(transform_val, 20000);
        pBand1HFillProperties->maFillColor.addTransformation(transform_val, 40000);
        pBand1VFillProperties->maFillColor.addTransformation(transform_val, 40000);
        pLastColFillProperties->maFillColor.addTransformation(transform_val, 60000);
        pFirstColFillProperties->maFillColor.addTransformation(transform_val, 60000);
    }
    else if (style_name == "Dark-Style-2")
    {
        setBorderLineType(pLastRowTopBorder, XML_solidFill);

        wholeTblTextColor.setSchemeClr(XML_dk1);
        firstRowTextColor.setSchemeClr(XML_lt1);

        pLastRowTopBorder->maLineFill.maFillColor.setSchemeClr(XML_dk1);

        if (accent_name.isEmpty())
            pFirstRowFillProperties->maFillColor.setSchemeClr(XML_dk1);
        else if (accent_name == "Accent1")
            pFirstRowFillProperties->maFillColor.setSchemeClr(XML_accent2);
        else if (accent_name == "Accent3")
            pFirstRowFillProperties->maFillColor.setSchemeClr(XML_accent4);
        else if (accent_name == "Accent5")
            pFirstRowFillProperties->maFillColor.setSchemeClr(XML_accent6);

        sal_Int32 accent_val;

        if (!accent_name.isEmpty())
            accent_val = resolveToken(mStyleIdMap.find(styleId)->second.second);
        else
            accent_val = XML_dk1;

        pWholeTblFillProperties->maFillColor.setSchemeClr(accent_val);
        pLastRowFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1HFillProperties->maFillColor.setSchemeClr(accent_val);
        pBand1VFillProperties->maFillColor.setSchemeClr(accent_val);

        pWholeTblFillProperties->maFillColor.addTransformation(XML_tint, 20000);
        pBand1HFillProperties->maFillColor.addTransformation(XML_tint, 40000);
        pBand1VFillProperties->maFillColor.addTransformation(XML_tint, 40000);
        pLastRowFillProperties->maFillColor.addTransformation(XML_tint, 20000);
    }

    // End to handle all style groups.

    // Create a TableStyle from handled properties.
    pTableStyle->getStyleId() = styleId;
    pTableStyle->getStyleName() = style_name;

    pTableStyle->getFirstRow().getTextBoldStyle() = bFirstRowTextBoldStyle;
    pTableStyle->getFirstCol().getTextBoldStyle() = bFirstColTextBoldStyle;
    pTableStyle->getLastCol().getTextBoldStyle() = bLastColTextBoldStyle;

    pTableStyle->getWholeTbl().getTextColor() = std::move(wholeTblTextColor);
    pTableStyle->getFirstRow().getTextColor() = std::move(firstRowTextColor);
    pTableStyle->getFirstCol().getTextColor() = std::move(firstColTextColor);
    pTableStyle->getLastRow().getTextColor() = std::move(lastRowTextColor);
    pTableStyle->getLastCol().getTextColor() = std::move(lastColTextColor);

    pTableStyle->getBand1H().getTextColor() = ::oox::drawingml::Color(); //band1HTextColor
    pTableStyle->getBand1V().getTextColor() = ::oox::drawingml::Color(); //band1VTextColor
    pTableStyle->getBand2H().getTextColor() = ::oox::drawingml::Color(); //band2HTextColor
    pTableStyle->getBand2V().getTextColor() = ::oox::drawingml::Color(); //band2VTextColor

    pTableStyle->getBackgroundFillProperties() = std::move(pTblBgFillProperties);
    pTableStyle->getWholeTbl().getFillProperties() = std::move(pWholeTblFillProperties);
    pTableStyle->getFirstRow().getFillProperties() = std::move(pFirstRowFillProperties);
    pTableStyle->getFirstCol().getFillProperties() = std::move(pFirstColFillProperties);
    pTableStyle->getLastRow().getFillProperties() = std::move(pLastRowFillProperties);
    pTableStyle->getLastCol().getFillProperties() = std::move(pLastColFillProperties);
    pTableStyle->getBand1H().getFillProperties() = std::move(pBand1HFillProperties);
    pTableStyle->getBand1V().getFillProperties() = std::move(pBand1VFillProperties);
    pTableStyle->getBand2H().getFillProperties() = std::move(pBand2HFillProperties);
    pTableStyle->getBand2V().getFillProperties() = std::move(pBand2VFillProperties);

    insertBorderLine(pTableStyle->getWholeTbl(), XML_left, pWholeTblLeftBorder);
    insertBorderLine(pTableStyle->getWholeTbl(), XML_right, pWholeTblRightBorder);
    insertBorderLine(pTableStyle->getWholeTbl(), XML_top, pWholeTblTopBorder);
    insertBorderLine(pTableStyle->getWholeTbl(), XML_bottom, pWholeTblBottomBorder);
    insertBorderLine(pTableStyle->getWholeTbl(), XML_insideV, pWholeTblInsideVBorder);
    insertBorderLine(pTableStyle->getWholeTbl(), XML_insideH, pWholeTblInsideHBorder);

    insertBorderLine(pTableStyle->getFirstRow(), XML_left, pFirstRowLeftBorder);
    insertBorderLine(pTableStyle->getFirstRow(), XML_right, pFirstRowRightBorder);
    insertBorderLine(pTableStyle->getFirstRow(), XML_top, pFirstRowTopBorder);
    insertBorderLine(pTableStyle->getFirstRow(), XML_bottom, pFirstRowBottomBorder);
    insertBorderLine(pTableStyle->getFirstRow(), XML_insideV, pFirstRowInsideVBorder);
    insertBorderLine(pTableStyle->getFirstRow(), XML_insideH, pFirstRowInsideHBorder);

    insertBorderLine(pTableStyle->getFirstCol(), XML_left, pFirstColLeftBorder);
    insertBorderLine(pTableStyle->getFirstCol(), XML_right, pFirstColRightBorder);
    insertBorderLine(pTableStyle->getFirstCol(), XML_top, pFirstColTopBorder);
    insertBorderLine(pTableStyle->getFirstCol(), XML_bottom, pFirstColBottomBorder);
    insertBorderLine(pTableStyle->getFirstCol(), XML_insideV, pFirstColInsideVBorder);
    insertBorderLine(pTableStyle->getFirstCol(), XML_insideH, pFirstColInsideHBorder);

    insertBorderLine(pTableStyle->getLastRow(), XML_left, pLastRowLeftBorder);
    insertBorderLine(pTableStyle->getLastRow(), XML_right, pLastRowRightBorder);
    insertBorderLine(pTableStyle->getLastRow(), XML_top, pLastRowTopBorder);
    insertBorderLine(pTableStyle->getLastRow(), XML_bottom, pLastRowBottomBorder);
    insertBorderLine(pTableStyle->getLastRow(), XML_insideV, pLastRowInsideVBorder);
    insertBorderLine(pTableStyle->getLastRow(), XML_insideH, pLastRowInsideHBorder);

    insertBorderLine(pTableStyle->getLastCol(), XML_left, pLastColLeftBorder);
    insertBorderLine(pTableStyle->getLastCol(), XML_right, pLastColRightBorder);
    insertBorderLine(pTableStyle->getLastCol(), XML_top, pLastColTopBorder);
    insertBorderLine(pTableStyle->getLastCol(), XML_bottom, pLastColBottomBorder);
    insertBorderLine(pTableStyle->getLastCol(), XML_insideV, pLastColInsideVBorder);
    insertBorderLine(pTableStyle->getLastCol(), XML_insideH, pLastColInsideHBorder);

    insertBorderLine(pTableStyle->getBand1H(), XML_left, pBand1HLeftBorder);
    insertBorderLine(pTableStyle->getBand1H(), XML_right, pBand1HRightBorder);
    insertBorderLine(pTableStyle->getBand1H(), XML_top, pBand1HTopBorder);
    insertBorderLine(pTableStyle->getBand1H(), XML_bottom, pBand1HBottomBorder);
    insertBorderLine(pTableStyle->getBand1H(), XML_insideV, pBand1HInsideVBorder);
    insertBorderLine(pTableStyle->getBand1H(), XML_insideH, pBand1HInsideHBorder);

    insertBorderLine(pTableStyle->getBand1V(), XML_left, pBand1VLeftBorder);
    insertBorderLine(pTableStyle->getBand1V(), XML_right, pBand1VRightBorder);
    insertBorderLine(pTableStyle->getBand1V(), XML_top, pBand1VTopBorder);
    insertBorderLine(pTableStyle->getBand1V(), XML_bottom, pBand1VBottomBorder);
    insertBorderLine(pTableStyle->getBand1V(), XML_insideV, pBand1VInsideVBorder);
    insertBorderLine(pTableStyle->getBand1V(), XML_insideH, pBand1VInsideHBorder);

    insertBorderLine(pTableStyle->getBand2H(), XML_left, pBand2HLeftBorder);
    insertBorderLine(pTableStyle->getBand2H(), XML_right, pBand2HRightBorder);
    insertBorderLine(pTableStyle->getBand2H(), XML_top, pBand2HTopBorder);
    insertBorderLine(pTableStyle->getBand2H(), XML_bottom, pBand2HBottomBorder);
    insertBorderLine(pTableStyle->getBand2H(), XML_insideV, pBand2HInsideVBorder);
    insertBorderLine(pTableStyle->getBand2H(), XML_insideH, pBand2HInsideHBorder);

    insertBorderLine(pTableStyle->getBand2V(), XML_left, pBand2VLeftBorder);
    insertBorderLine(pTableStyle->getBand2V(), XML_right, pBand2VRightBorder);
    insertBorderLine(pTableStyle->getBand2V(), XML_top, pBand2VTopBorder);
    insertBorderLine(pTableStyle->getBand2V(), XML_bottom, pBand2VBottomBorder);
    insertBorderLine(pTableStyle->getBand2V(), XML_insideV, pBand2VInsideVBorder);
    insertBorderLine(pTableStyle->getBand2V(), XML_insideH, pBand2VInsideHBorder);

    return pTableStyle;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
