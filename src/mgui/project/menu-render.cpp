//
// mgui/project/menu-render.cpp
// This file is part of Bombono DVD project.
//
// Copyright (c) 2008-2010 Ilya Murav'jov
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// 

#include <mgui/_pc_.h>

#include "menu-render.h"
#include "thumbnail.h"

#include <mgui/editor/render.h>
#include <mgui/editor/text.h>
#include <mgui/editor/kit.h>
#include <mgui/editor/bind.h> // DoRenderFTO()

#include <mgui/author/script.h> // IsMotion()
#include <mgui/img-factory.h>

#include <mbase/project/srl-common.h> // MakeColor()
#include <mlib/sdk/logger.h>
#include <mdemux/util.h>         // Mpeg::set_hms

#include <cmath> // std::ceil(), std::floor()

namespace Project
{

MenuPack::MenuPack(DataWare& dw): editor(0), thumbNeedUpdate(false)
{
    owner = dynamic_cast<MenuMD*>(&dw);
    ASSERT( owner );
}

void SimpleInitTextVis::Visit(TextObj& t_obj)
{
    EdtTextRenderer& t_rndr = FindTextRenderer(t_obj, cBuf);

    t_rndr.Init(cBuf.Canvas());
    t_rndr.SetCanvasBuf(&cBuf);
    t_rndr.DoLayout();
}

class ThumbRenderVis: public CommonRenderVis
{
    typedef CommonRenderVis MyParent;
    public:
                  ThumbRenderVis(RectListRgn& r_lst): MyParent(r_lst) { }

   virtual RefPtr<Gdk::Pixbuf> CalcBgShot();
   virtual  void  Visit(FrameThemeObj& fto);
};

RefPtr<Gdk::Pixbuf> ThumbRenderVis::CalcBgShot()
{
    return GetPrimaryShot(menuRgn->BgRef());
}

const Editor::ThemeData& FTOThumbData::GetTheme(std::string& thm_nm) 
{ 
    return Editor::GetThumbTheme(thm_nm); 
}

RefPtr<Gdk::Pixbuf> FTOThumbData::CalcSource(Project::MediaItem mi, const Point& sz)
{ 
    return GetCalcedShot(mi, sz); 
}

void ThumbRenderVis::Visit(FrameThemeObj& fto)
{
    FTOThumbData& pix_data = fto.GetData<FTOThumbData>();
    DoRenderFTO(drw.get(), fto, pix_data, cnvBuf->Transition());
}

CommonRgnListCleaner::CommonRgnListCleaner(CanvasBuf& cnv_buf): cnvBuf(cnv_buf) 
{ 
    cnvBuf.RecurseCount()++; 
}

void CommonRgnListCleaner::Clean()
{
    int& rc = cnvBuf.RecurseCount(); 
    rc--;
    // очищаем
    if( !rc )
    {
        OnClean();
        cnvBuf.RenderList().clear();
    }
}

// так как GetRenderedShot() может вызываться рекурсивно,
// то надо внимательно следить за "глобальными" в этом плане
// вещами:
// 1) FTOThumbData не должно в конструкторе создавать рамочное
//    изображение, иначе будет вставка кучи одинаковых FTOThumbData в Dataware;
// 2) MenuPack::renderLst должен удаляться в самом конце отрисовки, иначе
//    дорисовка (по крайней мере) последним ThumbRenderVis будет сломана
class MenuPackCleaner: public CommonRgnListCleaner
{
    typedef CommonRgnListCleaner MyParent;
    public:
                MenuPackCleaner(MenuPack& mp): MyParent(mp.thRgn.GetCanvasBuf()), mnPack(mp) {}
               ~MenuPackCleaner() { Clean(); }
    protected:
                MenuPack& mnPack;

  virtual void  OnClean();
};

static RefPtr<Gdk::Pixbuf> FramePixbuf(MenuPack& mp)
{
    return mp.thRgn.GetCanvasBuf().FramePixbuf();
}

// объединение прямоугольников в пределах plc 
namespace { bool MakeUnion(Rect& union_rect, RectListRgn& lst, const Rect& plc)
{
    bool is_first = true;
    for( RectListRgn::iterator itr = lst.begin(), end = lst.end(); itr != end; ++itr )
    {
        union_rect = is_first ? *itr : Union(union_rect, *itr) ;
        is_first = false;
    }
    if( !is_first )
        union_rect = Intersection(union_rect, plc);
    return !is_first;
} }

static void RenderThumbnail(MenuPack& mn_pack, RefPtr<Gdk::Pixbuf> src_pix, 
                            const Rect& plc, const Rect& union_rect)
{
    RefPtr<Gdk::Pixbuf> thumb = mn_pack.thumbPix;
    // *
    //RGBA::Scale(mnPack.thumbPix, src_pix);
    // *
    //RectListRgn one_lst;
    //one_lst.push_back(union_rect);
    //RGBA::RgnPixelDrawer drw(mnPack.thumbPix, &one_lst);
    //drw.ScalePixbuf(src_pix, plc);
    // * - самый быстрый вариант
    RGBA::ScalePixbuf(thumb, src_pix, plc, union_rect);

    Menu mn = mn_pack.Owner();
    StampFPEmblem(mn, thumb);

    // эмблема как признак анимационности
    if( IsMotion(mn) )
    {
        RefPtr<Gdk::Pixbuf> emblem = GetCheckEmblem(thumb, "emblems/tmp/applications-multimedia.png");
        RGBA::AlphaComposite(thumb, emblem, Point(thumb->get_width()-emblem->get_width()-2, 2));
    }
}

void RenderThumbnail(MenuPack& mn_pack)
{
    Rect plc = PixbufBounds(mn_pack.thumbPix);
    RenderThumbnail(mn_pack, FramePixbuf(mn_pack), plc, plc);
}

void MenuPackCleaner::OnClean()
{
    RefPtr<Gdk::Pixbuf> src_pix = FramePixbuf(mnPack);
    Rect plc = PixbufBounds(mnPack.thumbPix);
    RectListRgn& render_lst     = GetRenderList(mnPack.thRgn);

    // *
    RectListRgn t_lst;
    MapRectList(t_lst, render_lst, plc, PixbufSize(src_pix));
    Rect union_rect; // для скорости заменим на один
    if( MakeUnion(union_rect, t_lst, plc) )
        RenderThumbnail(mnPack, src_pix, plc, union_rect);

    // *
    if( mnPack.editor )
        mnPack.editor->DrawUpdate(render_lst);

    // *
    mnPack.thumbNeedUpdate = true;
}

MenuPack& UpdateMenuPack(Menu mn)
{
    MenuPack& mp = mn->GetData<MenuPack>();
    RectListRgn& render_lst = GetRenderList(mp.thRgn);
    if( render_lst.size() )
    {
        MenuPackCleaner lst_cleaner(mp);
        if( mp.editor )
            RenderEditor(*mp.editor, render_lst);
        else
        {
            ThumbRenderVis r_vis(render_lst);
            mp.thRgn.Accept(r_vis);
        }
    }
    return mp;
}

RefPtr<Gdk::Pixbuf> GetRenderedShot(Menu mn)
{
    return FramePixbuf(UpdateMenuPack(mn));
}

bool& UpdateMenuThumb(Menu mn)
{
    return UpdateMenuPack(mn).thumbNeedUpdate;
}

void SetCBDirty(CanvasBuf& cb)
{
    cb.RenderList().push_back(cb.FrameRect());
}

void SetMenuDirty(Menu mn)
{
    MenuPack& mp  = mn->GetData<MenuPack>();
    //CanvasBuf& cb = mp.thRgn.GetCanvasBuf();
    //cb.RenderList().push_back(cb.FrameRect());
    SetCBDirty(mp.thRgn.GetCanvasBuf());
}

static void CreateMenuThumbPix(MenuPack& mp)
{
    RefPtr<Gdk::Pixbuf>& thumb_pix = mp.thumbPix;
    ASSERT( !thumb_pix ); // только один раз создаем

    Point thumb_sz = CalcProportionSize4_3(mp.CnvBuf().Size(), BIG_THUMB_WDH); 
    thumb_pix = CreatePixbuf(thumb_sz);
}

void SetThumbControl(MenuPack& mp)
{
    mp.editor = 0;
    mp.thRgn.SetCanvasBuf(&mp.CnvBuf());

    // * инициализация
    SimpleInitTextVis titv(mp.CnvBuf());
    mp.thRgn.Accept(titv);
}

///////////////////////////////////////////////////////////////////////
// Преобразования MenuItem <-> MenuRegion 

static void LoadBackground(MenuRegion& menu_rgn, Menu mn)
{
    const MenuParams& prms = mn->Params(); //mn ? mn->Params() : AData().GetDefMP() ; 
    menu_rgn.GetParams()   = prms;
    menu_rgn.BgRef().SetLink(mn->BgRef());

    menu_rgn.bgSet = mn->bgSet;
    menu_rgn.BgColor().alpha = RGBA::Pixel::MaxClr; // форсируем непрозрачность

//     SimpleOverObj* bg_soo  = new SimpleOverObj;
//     SetOwnerMenu(bg_soo, mn.get());
//     bg_soo->SetPlacement(Rect0Sz(prms.Size()));
//     if( mn )
//         bg_soo->MediaItem().SetLink(mn->BgRef());
//     menu_rgn.Ins(*bg_soo);
}

TextObj* CreateEditorText(const std::string& text, Editor::TextStyle style, const Rect& plc)
{
    TextObj* obj = new TextObj;

    MEdt::CheckDescNonNull(style);
    obj->Load(text, plc, style);
    return obj;
}

FrameThemeObj* NewFTO(const FrameTheme& theme, const Rect& lct)
{
    FrameTheme ft = theme;
    // защита от пустых данных
    if( ft.themeName.empty() )
    {
        ft.isIcon    = false;    
        ft.themeName = "rect";
    }
    return new FrameThemeObj(ft, lct);
}

FrameThemeObj* CreateNewFTO(const FrameTheme& theme, const Rect& lct, MediaItem p_mi, 
                            bool hl_border)
{
    FrameThemeObj* fto = NewFTO(theme, lct);
    fto->PosterItem().SetLink(p_mi);
    fto->hlBorder = hl_border;
    
    return fto;
}

void AddMenuItem(MenuRegion& menu_rgn, Comp::MediaObj* obj)
{
    menu_rgn.Ins(*obj);
    SetOwnerMenu(obj, GetOwnerMenu(&menu_rgn));
}

void ClearMenuSavedData(Menu mn)
{
    // * очищаем хранительный вариант меню
    mn->BgRef() = 0;
    mn->List().clear();
}

static void LoadAddMI(MenuRegion& menu_rgn, Comp::MediaObj* obj, MenuItem mi)
{
    obj->MediaItem().SetLink(mi->Ref());
    obj->PlayAll() = mi->playAll;
    AddMenuItem(menu_rgn, obj);
}

void LoadMenu(MenuRegion& menu_rgn, Menu mn)
{
    ASSERT( mn );
    menu_rgn.Clear();
    SetOwnerMenu(&menu_rgn, mn.get());

    // * параметры и фон
    LoadBackground(menu_rgn, mn);

    // * пункты меню
    MenuMD::ListType& lst = mn->List();
    for( MenuMD::Itr itr = lst.begin(), end = lst.end(); itr != end; ++itr )
    {
        MenuItem mi = *itr;
        if( TextItemMD* txt_mi = dynamic_cast<TextItemMD*>(mi.get()) )
        {
            std::string text = txt_mi->mdName;
            ASSERT( !text.empty() );

            Editor::TextStyle style(Pango::FontDescription(txt_mi->FontDesc()), txt_mi->IsUnderlined(), 
                                    MakeColor(txt_mi->Color()));
            TextObj* txt_obj = CreateEditorText(text, style, txt_mi->Placement());
            
            LoadAddMI(menu_rgn, txt_obj, txt_mi);
        }
        else if( FrameItemMD* frame_mi = dynamic_cast<FrameItemMD*>(mi.get()) )
        {
            FrameThemeObj* fto = CreateNewFTO(frame_mi->Theme(), frame_mi->Placement(),
                                              frame_mi->Poster(), frame_mi->hlBorder);
            LoadAddMI(menu_rgn, fto, frame_mi);
        }
        else
            ASSERT(0);
    }

    ClearMenuSavedData(mn);
}

void SaveMenuItem(MenuItem mi, Comp::MediaObj* m_obj)
{
    mi->Placement() = m_obj->Placement();
    mi->Ref()       = m_obj->MediaItem();
    mi->playAll     = m_obj->PlayAll();
}

void SaveMenu(Menu mn, Menu orig_mn)
{
    ASSERT( mn && orig_mn );
    MenuRegion& m_rgn = GetMenuRegion(orig_mn);
    // * меню
    mn->Params() = m_rgn.GetParams();
    mn->BgRef()  = m_rgn.BgRef();
    mn->bgSet    = m_rgn.bgSet;

    MenuItem mi;
    for( MenuRegion::Itr itr = m_rgn.List().begin(), end = m_rgn.List().end(); itr != end; ++itr )
    {
        Comp::Object* obj = *itr;
        if( TextObj* t_obj = dynamic_cast<TextObj*>(obj) )
        {
            TextItemMD* t_itm = new TextItemMD(mn.get());
            t_itm->mdName     = t_obj->Text();
            const Editor::TextStyle& style = t_obj->Style();
            t_itm->FontDesc()     = style.fntDsc.to_string();
            t_itm->IsUnderlined() = style.isUnderlined;
            t_itm->Color()        = ToString(style.color);

            SaveMenuItem(t_itm, t_obj);
        }
        else if( FrameThemeObj* frame_obj = dynamic_cast<FrameThemeObj*>(obj) )
        {
            FrameItemMD* f_itm = new FrameItemMD(mn.get());
            f_itm->Theme()  = frame_obj->Theme();
            f_itm->Poster() = frame_obj->PosterItem();
            f_itm->hlBorder = frame_obj->hlBorder;

            SaveMenuItem(f_itm, frame_obj);
        }
        else
            ASSERT(0);
    }
}

void SaveMenu(Menu mn)
{
    SaveMenu(mn, mn);
}

// Преобразования MenuItem <-> MenuRegion 
///////////////////////////////////////////////////////////////////////

void OpenMenu(Menu mn)
{
    RefPtr<Gdk::Pixbuf> shot = GetCacheShot(mn);
    MenuPack& mp = mn->GetData<MenuPack>();

    // * PixCanvasBuf
    Rect plc = Rect0Sz(PixbufSize(shot));
    mp.CnvBuf().Set(shot, Planed::Transition(plc, mn->Params().GetSize()));

    // * создание
    LoadMenu(mp.thRgn, mn);
    CreateMenuThumbPix(mp);
    
    // *
    SetThumbControl(mp);
    SetMenuDirty(mn);
}

} // namespace Project

static inline void MapRect(Rect& rct, double r_x, double r_y, const Rect& plc, RectListRgn& dst)
{
    rct.lft = (int)std::floor(rct.lft * r_x + plc.lft);
    rct.top = (int)std::floor(rct.top * r_y + plc.top);
    rct.rgt = (int)std::ceil(rct.rgt * r_x + plc.lft);
    rct.btm = (int)std::ceil(rct.btm * r_y + plc.top);

    if( !rct.IsNull() )
        dst.push_back(rct);
}

// рассчитываем положения прямоугольников при отображении Rect(Point(0, 0), src_sz) -> plc
void MapRectList(RectListRgn& dst, RectListRgn& src, const Rect& plc, const Point& src_sz)
{
    if( IsNullSize(src_sz) )
        return;
    Point plc_sz(plc.Size());
    double r_x = plc_sz.x/(double)src_sz.x;
    double r_y = plc_sz.y/(double)src_sz.y;

    Rect rct;
    if( &src != &dst )
        for( RLRIterType itr = src.begin(), end = src.end(); itr != end; ++itr )
        {
            rct = *itr;
            MapRect(rct, r_x, r_y, plc, dst);
        }
    else
        for( int i = 0, cnt = (int)src.size(); i<cnt; i++ ) // чтобы resize() в векторе dst не сбил itr
        {
            rct = src[i];
            MapRect(rct, r_x, r_y, plc, dst);
        }
}

