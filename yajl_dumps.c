/* See LICENSE file for copyright and license details. */
#include "yajl_dumps.h"
#include <stdint.h>

int
dump_tag(yajl_gen gen, const char* name, const int tag_mask)
{
    YMAP(
        YSTR("bit_mask");
    YINT(tag_mask);
    YSTR("name");
    YSTR(name);
    );
    return 0;
}

int
dump_tags(yajl_gen gen, const char* tags[], int tags_len)
{
    YARR(

    for (int i = 0; i < tags_len; i++)
        dump_tag(gen, tags[i], 1 << i);
    );
    return 0;
}

int
dump_client(yajl_gen gen, Client* c)
{
    if (!c)
    {
        YNULL();
        return 0;
    }

    YMAP(
        YSTR("name");
    YSTR(c->name);
    YSTR("tags");
    YINT(c->tags);
    YSTR("window_id");
    YINT(c->win);
    YSTR("monitor_number");
    YINT(c->mon->num);

    YSTR("geometry");
    YMAP(
        YSTR("current");
    YMAP(
        YSTR("x");
    YINT(c->x);
    YSTR("y");
    YINT(c->y);
    YSTR("width");
    YINT(c->w);
    YSTR("height");
    YINT(c->h);
    );
    YSTR("old");
    YMAP(
        YSTR("x");
    YINT(c->oldx);
    YSTR("y");
    YINT(c->oldy);
    YSTR("width");
    YINT(c->oldw);
    YSTR("height");
    YINT(c->oldh);
    );
    );

    YSTR("size_hints");
    YMAP(
        YSTR("base");
    YMAP(
        YSTR("width");
    YINT(c->basew);
    YSTR("height");
    YINT(c->baseh);
    );
    YSTR("step");
    YMAP(
        YSTR("width");
    YINT(c->incw);
    YSTR("height");
    YINT(c->inch);
    );
    YSTR("max");
    YMAP(
        YSTR("width");
    YINT(c->maxw);
    YSTR("height");
    YINT(c->maxh);
    );
    YSTR("min");
    YMAP(
        YSTR("width");
    YINT(c->minw);
    YSTR("height");
    YINT(c->minh);
    );
    YSTR("aspect_ratio");
    YMAP(
        YSTR("min");
    YDOUBLE(c->mina);
    YSTR("max");
    YDOUBLE(c->maxa);
    );
    );

    YSTR("border_width");
    YMAP(
        YSTR("current");
    YINT(c->bw);
    YSTR("old");
    YINT(c->oldbw);
    );

    YSTR("states");
    YMAP(
        YSTR("is_fixed");
    YBOOL(c->isfixed);
    YSTR("is_floating");
    YBOOL(c->isfloating);
    YSTR("is_urgent");
    YBOOL(c->isurgent);
    YSTR("never_focus");
    YBOOL(c->neverfocus);
    YSTR("old_state");
    YBOOL(c->oldstate);
    YSTR("is_fullscreen");
    YBOOL(c->isfullscreen);
    );
    );
    return 0;
}

int
dump_monitors(yajl_gen gen, Monitor* mons, Monitor* selmon)
{
    yajl_gen_array_open(gen);
    for (Monitor* m = mons; m; m = m->next)
    {
        unsigned int occ = 0, tagset = m->tagset[m->seltags];
        for (Client* c = m->clients; c; c = c->next)
            occ |= c->tags;

        yajl_gen_map_open(gen);

        YSTR("master_factor");
        YDOUBLE(m->mfact);
        YSTR("num_master");
        YINT(m->nmaster);
        YSTR("num");
        YINT(m->num);
        YSTR("is_selected");
        YBOOL(m == selmon);

        YSTR("monitor_geometry");
        YMAP(
            YSTR("x");
        YINT(m->mx);
        YSTR("y");
        YINT(m->my);
        YSTR("width");
        YINT(m->mw);
        YSTR("height");
        YINT(m->mh);
        );

        YSTR("window_geometry");
        YMAP(
            YSTR("x");
        YINT(m->wx);
        YSTR("y");
        YINT(m->wy);
        YSTR("width");
        YINT(m->ww);
        YSTR("height");
        YINT(m->wh);
        );

        YSTR("tagset");
        YMAP(
            YSTR("current");
        YINT(m->tagset[m->seltags]);
        YSTR("old");
        YINT(m->tagset[m->seltags ^ 1]);
        );

        YSTR("tag_state");
        YMAP(
            YSTR("occupied");
        YINT(occ);
        YSTR("active");
        YINT(tagset);
        );

        YSTR("layout");
        YMAP(
            YSTR("symbol");
        YMAP(
            YSTR("current");
        YSTR(m->ltsymbol);
        YSTR("old");
        YSTR(m->ltsymbol);
        );
        YSTR("address");
        YMAP(
            YSTR("current");
        YINT((uintptr_t)m->lt[m->sellt]);
        YSTR("old");
        YINT((uintptr_t)m->lt[m->sellt ^ 1]);
        );
        );

        YSTR("clients");
        YMAP(
            YSTR("selected");
        YINT(m->sel ? m->sel->win : 0);
        YSTR("stack");
        YARR(

        for (Client* c = m->stack; c; c = c->snext)
            YINT(c->win);
        );
        YSTR("all");
        YARR(

        for (Client* c = m->clients; c; c = c->next)
            YINT(c->win);
        );
        );

        yajl_gen_map_close(gen);
    }
    yajl_gen_array_close(gen);
    return 0;
}

int
dump_layouts(yajl_gen gen, const Layout layouts[], int layouts_len)
{
    YARR(

    for (int i = 0; i < layouts_len; i++)
    {
        YMAP(
            YSTR("symbol");
        YSTR(layouts[i].symbol ? layouts[i].symbol : "");
        YSTR("address");
        YINT((uintptr_t) & layouts[i]);
        );
    }
    );
    return 0;
}

int
dump_error(yajl_gen gen, const char* reason)
{
    YMAP(
        YSTR("error");
    YSTR(reason);
    );
    return 0;
}
