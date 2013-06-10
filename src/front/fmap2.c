/***************************************************************
FronTier is a set of libraries that implements differnt types of 
Front Traking algorithms. Front Tracking is a numerical method for 
the solution of partial differential equations whose solutions have 
discontinuities.  


Copyright (C) 1999 by The University at Stony Brook. 
 

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************/


/*
*				fmap2.c:
*
*	Copyright 1999 by The University at Stony Brook, All rights reserved.
*/

#include <front/fdecs.h>		/* includes int.h, table.h */

LOCAL double cell_frac_comp2(double***,double***,COMPONENT**,COMPONENT);
LOCAL double corner_cell_frac(double***,double***,COMPONENT**,
			COMPONENT,boolean);
LOCAL void rot_clockwise_90(double***,double***,int**);
LOCAL void FT_ComputeGridVolumeFraction2d(Front*,COMPONENT,POINTER*);
LOCAL double FT_ComputeTotalVolumeFraction2d(Front*,COMPONENT);
LOCAL void forward_curve_seg_len_constr(CURVE*,BOND*,BOND*,int,double);
LOCAL void backward_curve_seg_len_constr(CURVE*,BOND*,BOND*,int,double);

EXPORT double FT_ComputeTotalVolumeFraction(
	Front *front,
	COMPONENT comp_of_vol)
{
	switch (front->rect_grid->dim)
	{
	case 2:
	    return FT_ComputeTotalVolumeFraction2d(front,comp_of_vol);
	case 3:
	    screen("FT_ComputeTotalVolumeFraction3d() not implemented!\n");
	    return 0.0;
	}
}	/* end FT_ComputeTotalVolumeFraction */

LOCAL double FT_ComputeTotalVolumeFraction2d(
	Front *front,
	COMPONENT comp_of_vol)
{
	double **grid_vol_frac;
	INTERFACE *grid_intfc = front->grid_intfc;
	RECT_GRID *top_grid = &topological_grid(grid_intfc);
	int *lbuf = front->rect_grid->lbuf;
	int *ubuf = front->rect_grid->ubuf;
	int *top_gmax = top_grid->gmax;
	double total_vol = 0.0;
	int i,j,imin,jmin,imax,jmax;

	FT_ComputeGridVolumeFraction2d(front,comp_of_vol,
			(POINTER*)&grid_vol_frac);
	imin = (lbuf[0] == 0) ? 1 : lbuf[0];
	jmin = (lbuf[1] == 0) ? 1 : lbuf[1];
        imax = (ubuf[0] == 0) ? top_gmax[0] - 1 : top_gmax[0] - ubuf[0];
        jmax = (ubuf[1] == 0) ? top_gmax[1] - 1 : top_gmax[1] - ubuf[1];
	for (i = imin; i <= imax; ++i)
	for (j = jmin; j <= jmax; ++j)
	    total_vol += grid_vol_frac[i][j];

	pp_global_sum(&total_vol,1);
	FT_FreeThese(1,grid_vol_frac);
	return total_vol;
}	/* end FT_ComputeTotalVolumeFraction2d */

EXPORT void FT_ComputeGridVolumeFraction(
	Front *front,
	COMPONENT comp_of_vol,
	POINTER *grid_vol_frac)
{
	switch (front->rect_grid->dim)
	{
	case 2:
	    FT_ComputeGridVolumeFraction2d(front,comp_of_vol,grid_vol_frac);
	    return;
	case 3:
	    screen("FT_ComputeVolumeFraction3d() not implemented!\n");
	    return;
	}
}	/* end FT_ComputeGridVolumeFraction */

LOCAL void FT_ComputeGridVolumeFraction2d(
	Front *front,
	COMPONENT comp_of_vol,
	POINTER *grid_vol_frac)
{
	int i,j,i1,j1,ii,jj;
	INTERFACE *grid_intfc = front->grid_intfc;
	RECT_GRID *top_grid = &topological_grid(grid_intfc);
	int *lbuf = front->rect_grid->lbuf;
	int *ubuf = front->rect_grid->ubuf;
	int *gmax = top_grid->gmax;
	double *L = top_grid->L;
	double *U = top_grid->U;
	double *h = top_grid->h;
	int dim = grid_intfc->dim;
	struct Table *T = table_of_interface(grid_intfc);
	COMPONENT *top_comp = T->components;
	double **vol_frac_2d;
	static double ***corner,***edge_crx;
	static COMPONENT **corner_comp;
	int ic,num_comp,icoords[MAXD];
	POINTER state;
	HYPER_SURF *hs;

	if (corner == NULL)
	{
	    FT_TriArrayMemoryAlloc((POINTER*)&corner,2,2,MAXD,sizeof(double));
	    FT_TriArrayMemoryAlloc((POINTER*)&edge_crx,2,2,MAXD,sizeof(double));
	    FT_MatrixMemoryAlloc((POINTER*)&corner_comp,2,2,sizeof(int));
	}
	FT_MatrixMemoryAlloc((POINTER*)&vol_frac_2d,gmax[0],gmax[1],
				sizeof(double));
	for (i = 0; i < gmax[0]; ++i)
	for (j = 0; j < gmax[1]; ++j)
	{
	    num_comp = 0;
	    for (i1 = 0; i1 < 2; ++i1)
	    for (j1 = 0; j1 < 2; ++j1)
	    {
		ii = i+i1;
		jj = j+j1;
	    	corner[i1][j1][0] = L[0] + ii*h[0];
	    	corner[i1][j1][1] = L[0] + jj*h[0];
	    	ic = d_index2d(ii,jj,gmax);
	    	corner_comp[i1][j1] = top_comp[ic];
		if (top_comp[ic] == comp_of_vol) num_comp++;
	    }
	
	    for (i1 = 0; i1 < 2; ++i1)
	    {
	    	if (corner_comp[0][i1] != corner_comp[1][i1])
		{
		    icoords[0] = i;
		    icoords[1] = j + i1;
		    FT_StateStructAtGridCrossing(front,grid_intfc,icoords,EAST,
				corner_comp[0][i1],&state,&hs,edge_crx[0][i1]);
		}
	    	if (corner_comp[i1][0] != corner_comp[i1][1])
		{
		    icoords[0] = i + i1;
		    icoords[1] = j;
		    FT_StateStructAtGridCrossing(front,grid_intfc,icoords,NORTH,
				corner_comp[i1][0],&state,&hs,edge_crx[1][i1]);
		}
	    }
	    switch (num_comp)
	    {
	    case 0:
		vol_frac_2d[i][j] = 0.0;
		break;
	    case 1:
		vol_frac_2d[i][j] = corner_cell_frac(corner,edge_crx,
				corner_comp,comp_of_vol,NO);
		break;
	    case 2:
		vol_frac_2d[i][j] = cell_frac_comp2(corner,edge_crx,
				corner_comp,comp_of_vol);
		break;
	    case 3:
		vol_frac_2d[i][j] = h[0]*h[1] - corner_cell_frac(corner,
				edge_crx,corner_comp,comp_of_vol,YES);
		break;
	    case 4:
		vol_frac_2d[i][j] = h[0]*h[1];
		break;
	    }
	}
	*grid_vol_frac = (POINTER)vol_frac_2d;
}	/* end FT_ComputeGridVolumeFraction2d */

#define	the_corner(comp,comp_of_vol,use_complement)  \
	(((comp) == (comp_of_vol) && !(use_complement)) || \
	 ((comp) != (comp_of_vol) && (use_complement)))

LOCAL double corner_cell_frac(
	double ***corner,
	double ***edge,
	COMPONENT **comp,
	COMPONENT comp_of_vol,
	boolean use_complement)
{
	int i;
	double a,b;
	/* All triangle cases */
	for (i = 0; i < 4; ++i)
	{
	    if (the_corner(comp[0][0],comp_of_vol,use_complement))
	    {
	    	a = edge[0][0][0] - corner[0][0][0];
	    	b = edge[1][0][1] - corner[0][0][1];
		return 0.5*a*b;
	    }
	    rot_clockwise_90(corner,edge,comp);
	}
}	/* end corner_cell_frac */

LOCAL double cell_frac_comp2(
	double ***corner,
	double ***edge,
	COMPONENT **comp,
	COMPONENT comp_of_vol)
{
	int i;
	double a,b,h;
	double volume = 0.0;
	/* Traingle cases */
	if ((the_corner(comp[0][0],comp_of_vol,NO) &&
	     the_corner(comp[1][1],comp_of_vol,NO)) ||
	    (the_corner(comp[1][0],comp_of_vol,NO) &&
	     the_corner(comp[0][1],comp_of_vol,NO)))
	{
	    for (i = 0; i < 4; ++i)
	    {
	    	if (the_corner(comp[0][0],comp_of_vol,NO))
	    	{
	    	    a = edge[0][0][0] - corner[0][0][0];
	    	    b = edge[1][0][1] - corner[0][0][1];
		    volume += 0.5*a*b;
	    	}
	    	rot_clockwise_90(corner,edge,comp);
	    }
	    return volume;
	}
	/* Trapezoidal cases */
	else 
	{
	    for (i = 0; i < 4; ++i)
	    {
	    	if (the_corner(comp[0][0],comp_of_vol,NO) &&
	    	    the_corner(comp[1][0],comp_of_vol,NO))
		{
		    a = edge[1][0][1] - corner[0][0][1];
		    b = edge[1][1][1] - corner[1][0][1];
		    h = corner[1][0][0] - corner[0][0][0];
		    return 0.5*(a + b)*h;
		}
	    	rot_clockwise_90(corner,edge,comp);
	    }
	}
}	/* end cell_frac_comp2 */

LOCAL void rot_clockwise_90(
	double ***corner,
	double ***edge,
	int **comp)
{
	COMPONENT tmp_comp;
	double tmp_corner[MAXD];
	double tmp_edge[MAXD];
	int i;

	tmp_comp = comp[0][0];
	comp[0][0] = comp[1][0];
	comp[1][0] = comp[1][1];
	comp[1][1] = comp[0][1];
	comp[0][1] = tmp_comp;
	for (i = 0; i < 2; ++i)
	{
	    tmp_corner[i] = corner[0][0][i];
	    corner[0][0][i] = corner[1][0][i];
	    corner[1][0][i] = corner[1][1][i];
	    corner[1][1][i] = corner[0][1][i];
	    corner[0][1][i] = tmp_corner[i];
	    tmp_edge[i] = edge[0][0][i];
	    edge[0][0][i] = edge[1][1][i];
	    edge[1][1][i] = edge[0][1][i];
	    edge[0][1][i] = edge[1][0][i];
	    edge[1][0][i] = tmp_edge[i];
	}
}	/* end rot_clockwise_90 */

EXPORT double FT_CurveLength(
        CURVE *c)
{
	return curve_length(c);
}	/* end FT_CurveLength */

EXPORT void FT_CurveSegLengthConstr(
        CURVE *c,
        BOND *bs,
        BOND *be,
        int nbds,
        double seg_length,
        REDISTRIBUTION_DIRECTION dir)
{
        switch (dir)
        {
        case FORWARD_REDISTRIBUTION:
	    forward_curve_seg_len_constr(c,bs,be,nbds,seg_length);
            return;
        case BACKWARD_REDISTRIBUTION:
	    backward_curve_seg_len_constr(c,bs,be,nbds,seg_length);
            return;
        }
}       /* end FT_CurveSegLengthConstr */


static	void forward_curve_seg_len_constr(
	CURVE		*c,
	BOND		*bs,
	BOND		*be,
	int		nbds,
	double		seg_len)
{
	BOND		*b, *bstart, *bend;
	double		b_len, sc_len, offset, total_offset, s, oms;
	double		coords[MAXD];
	double		space_tol;
	boolean		reset_bend;

	b_len = seg_len/(double)nbds;
	space_tol = b_len*1.0e-8;


	offset = b_len;		total_offset = seg_len;
	bstart = bs;		bend = be;
	while (bstart != bend)
	{
	    b = bstart;
	    while ((sc_len = bond_length(b)) < offset - space_tol)
	    {
	    	offset -= sc_len;
		if (b == bend) break;
	    	b = b->next;
	    }
	    if (sc_len > offset + space_tol)
	    {
	    	s = offset/sc_len;	oms = 1.0 - s;
	    	coords[0] = oms*Coords(b->start)[0] + s*Coords(b->end)[0];
	    	coords[1] = oms*Coords(b->start)[1] + s*Coords(b->end)[1];
		if (b == bend) reset_bend = YES;
		else reset_bend = NO;
	    	if (insert_point_in_bond(Point(coords),b,c) !=
		    FUNCTION_SUCCEEDED)
	    	{
	    	    screen("ERROR in forward_curve_seg_len_constr(), "
	    	           "insert_point_in_bond failed\n");
	    	    clean_up(ERROR);
	    	}
		if (reset_bend) bend = b->next;
	    }
	    replace_curve_seg_by_bond(c,bstart,b);
	    total_offset -= b_len;
	    bstart = bstart->next;
	    offset = b_len;
	    if (total_offset < b_len - space_tol) break;
	}
	if (bstart == bend)
	{
	    if (total_offset < space_tol)
	    {
		Coords(bend->end)[0] = Coords(bstart->start)[0];
		Coords(bend->end)[1] = Coords(bstart->start)[1];
		bstart = bstart->prev;
	    	replace_curve_seg_by_bond(c,bstart,bend);
	    }
	    else
	    {
	    	sc_len = bond_length(bend);
	    	s = total_offset/sc_len;      oms = 1.0 - s;
	    	Coords(bend->end)[0] = Coords(bend->start)[0] + s*
			(Coords(bend->end)[0] - Coords(bend->start)[0]);
	    	Coords(bend->end)[1] = Coords(bend->start)[1] + s*
			(Coords(bend->end)[1] - Coords(bend->start)[1]);
	    	set_bond_length(bend,2);
		b = bend;
		while (total_offset/b_len > 2.0 - space_tol)
		{
	    	    s = b_len/total_offset;	oms = 1.0 - s;
	    	    coords[0] = oms*Coords(b->start)[0] + s*Coords(b->end)[0];
	    	    coords[1] = oms*Coords(b->start)[1] + s*Coords(b->end)[1];
	    	    if (insert_point_in_bond(Point(coords),b,c) !=
		    	FUNCTION_SUCCEEDED)
	    	    {
	    	    	screen("ERROR in forward_curve_seg_len_constr(), "
	    	           	"insert_point_in_bond failed\n");
	    	    	clean_up(ERROR);
	    	    }
		    total_offset -= b_len;
		    b = b->next;
		}
	    }
	}
	else
	{
	    if (total_offset < space_tol && bstart != NULL)
	    {
		Coords(bend->end)[0] = Coords(bstart->start)[0];
		Coords(bend->end)[1] = Coords(bstart->start)[1];
		bstart = bstart->prev;
	    	replace_curve_seg_by_bond(c,bstart,bend);
	    }
	}
	return;
}		/*end forward_curve_seg_len_constr*/

static	void backward_curve_seg_len_constr(
	CURVE		*c,
	BOND		*bs,
	BOND		*be,
	int		nbds,
	double		seg_len)
{
	BOND		*b, *bstart, *bend;
	double		b_len, sc_len, offset, total_offset, s, oms;
	double		coords[MAXD];
	double		space_tol;
	boolean		reset_bend;

	b_len = seg_len/(double)nbds;
	space_tol = b_len*1.0e-8;


	offset = b_len;		total_offset = seg_len;
	bstart = bs;		bend = be;
	while (bstart != bend)
	{
	    b = bend;
	    while ((sc_len = bond_length(b)) < offset - space_tol)
	    {
	    	offset -= sc_len;
		if (b == bstart) break;
	    	b = b->prev;
	    }
	    if (sc_len > offset + space_tol)
	    {
	    	s = offset/sc_len;	oms = 1.0 - s;
	    	coords[0] = oms*Coords(b->end)[0] + s*Coords(b->start)[0];
	    	coords[1] = oms*Coords(b->end)[1] + s*Coords(b->start)[1];
		if (b == bend) reset_bend = YES;
		else reset_bend = NO;
	    	if (insert_point_in_bond(Point(coords),b,c) !=
		    FUNCTION_SUCCEEDED)
	    	{
	    	    screen("ERROR in backward_curve_seg_len_constr(), "
	    	           "insert_point_in_bond failed\n");
	    	    clean_up(ERROR);
	    	}
		b = b->next;
		if (reset_bend) bend = b;
	    }
	    replace_curve_seg_by_bond(c,b,bend);
	    total_offset -= b_len;
	    bend = b->prev;
	    offset = b_len;
	    if (total_offset < b_len - space_tol) break;
	}
	if (bstart == bend)
	{
	    if (total_offset < space_tol)
	    {
		Coords(bstart->start)[0] = Coords(bstart->end)[0];
		Coords(bstart->start)[1] = Coords(bstart->end)[1];
	    	replace_curve_seg_by_bond(c,bstart,bstart->next);
	    }
	    else
	    {
	    	sc_len = bond_length(bstart);
	    	s = total_offset/sc_len;      oms = 1.0 - s;
	    	Coords(bstart->start)[0] = Coords(bend->end)[0] + s*
			(Coords(bend->start)[0] - Coords(bend->end)[0]);
	    	Coords(bstart->start)[1] = Coords(bend->end)[1] + s*
			(Coords(bend->start)[1] - Coords(bend->end)[1]);
	    	set_bond_length(bstart,2);
		b = bstart;
		while (total_offset/b_len > 2.0 - space_tol)
		{
	    	    s = b_len/total_offset;	oms = 1.0 - s;
	    	    coords[0] = oms*Coords(b->start)[0] + s*Coords(b->end)[0];
	    	    coords[1] = oms*Coords(b->start)[1] + s*Coords(b->end)[1];
	    	    if (insert_point_in_bond(Point(coords),b,c) !=
		    	FUNCTION_SUCCEEDED)
	    	    {
	    	    	screen("ERROR in forward_curve_seg_len_constr(), "
	    	           	"insert_point_in_bond failed\n");
	    	    	clean_up(ERROR);
	    	    }
		    total_offset -= b_len;
		}
	    }
	}
	else
	{
	    if (total_offset < space_tol && bend != NULL)
	    {
		Coords(bstart->start)[0] = Coords(bend->end)[0];
		Coords(bstart->start)[1] = Coords(bend->end)[1];
		bend = bend->next;
	    	replace_curve_seg_by_bond(c,bstart,bend);
	    }
	}
	return;
}		/*end backward_curve_seg_len_constr*/

EXPORT	void FT_SetGlobalIndex(
	Front *front)
{
	INTERFACE *intfc = front->interf;
	RECT_GRID *gr = front->rect_grid;
	SURFACE **s;
	CURVE **c;
	NODE **n;
	TRI *t;
	BOND *b;
	POINT *p;
	double *L = gr->L;
	double *U = gr->U;
	boolean is_subdomain_bdry[MAXD][2];
	int i,j,iv,dim = gr->dim;
	int gindex = 0;
	int cindex = 0;
	int sindex = 0;
	boolean out_domain;

	reset_sort_status(intfc);
	for (i = 0; i < dim; ++i)
	for (j = 0; j < 2; ++j)
	{
	    if (rect_boundary_type(intfc,i,j) == SUBDOMAIN_BOUNDARY)
		is_subdomain_bdry[i][j] = YES;
	    else
		is_subdomain_bdry[i][j] = NO;
	}

	for (n = intfc->nodes; n && *n; ++n)
	{
	    p = (*n)->posn;
	    if (sorted(p)) continue;
	    sorted(p) = YES;
	    out_domain = NO;
	    for (i = 0; i < dim; ++i)
	    {
		if ((is_subdomain_bdry[i][0] && Coords(p)[i] < L[i]) ||
		    (is_subdomain_bdry[i][1] && Coords(p)[i] >= U[i]))
		    out_domain = YES;
	    }
	    if (out_domain) continue;
	    Gindex(p) = gindex++;
	}
	for (c = intfc->curves; c && *c; ++c)
	{
	    for (b = (*c)->first; b != (*c)->last; b = b->next)
	    {
		p = b->end;
	    	if (sorted(p)) continue;
	    	sorted(p) = YES;
	    	out_domain = NO;
	    	for (i = 0; i < dim; ++i)
	    	{
		    if ((is_subdomain_bdry[i][0] && Coords(p)[i] < L[i]) ||
		    	(is_subdomain_bdry[i][1] && Coords(p)[i] >= U[i]))
		    	out_domain = YES;
	    	}
	    	if (out_domain) continue;
	    	Gindex(p) = gindex++;
	    }
	}
	cindex = 0;
	for (c = intfc->curves; c && *c; ++c)
	{
	    Gindex(*c) = cindex++;
	}

	if (dim < 3) return;

	for (s = intfc->surfaces; s && *s; ++s)
	{
	    for (t = first_tri(*s); !at_end_of_tri_list(t,*s); t = t->next)
	    {
		for (iv = 0; iv < 3; ++iv)
		{
		    p = Point_of_tri(t)[iv];
	    	    if (sorted(p)) continue;
	    	    sorted(p) = YES;
		    out_domain = NO;
		    for (i = 0; i < dim; ++i)
		    {
			if ((is_subdomain_bdry[i][0] && Coords(p)[i] < L[i]) ||
			    (is_subdomain_bdry[i][1] && Coords(p)[i] >= U[i]))
			    out_domain = YES;
		    }
		    if (out_domain) continue;
		    Gindex(p) = gindex++;
		}
	    }
	}
	sindex = 0;
	for (s = intfc->surfaces; s && *s; ++s)
	    Gindex(*s) = sindex++;
}	/* end FT_SetGlobalIndex */

EXPORT	CURVE *FT_MakePointArrayCurve(
	Front *front,
	int num_points,
	double **point_array,
	COMPONENT   neg_comp,
        COMPONENT   pos_comp,
	boolean is_closed_curve,
	int w_type)
{
	CURVE *curve;
	curve = make_array_curve(front->interf,neg_comp,pos_comp,num_points,
			point_array,is_closed_curve);
	wave_type(curve) = w_type;
	if (is_closed_curve)
	    node_type(curve->start) = CLOSED_NODE;
	return curve;
}	/* end FT_MakePointArrayCurve */
		

EXPORT	void FT_MakeEllipticSurf(
	Front *front,
	double *center,
	double *radius,
	COMPONENT   neg_comp,
        COMPONENT   pos_comp,
	int w_type,
	int refinement_level,
	SURFACE **surf)
{
	RECT_GRID *rgr = front->rect_grid;
	RECT_GRID box_rg;
	ELLIP_PARAMS ellip_params;
	int i,dim = rgr->dim;
	double L[MAXD],U[MAXD],dh;
	int gmax[MAXD];
	double *h = rgr->h;

	for (i = 0; i < dim; ++i)
	{
	    ellip_params.cen[i] = center[i];
	    ellip_params.rad[i] = radius[i];
	    L[i] = center[i] - radius[i];
	    U[i] = center[i] + radius[i];
	    gmax[i] = rint((U[i] - L[i])/h[i]);
	    dh = gmax[i]*h[i] - (U[i] - L[i]);
	    L[i] -= 0.5*dh;
	    U[i] += 0.5*dh;
	    gmax[i] = refinement_level*rint((U[i] - L[i])/h[i]);
	}
	set_box_rect_grid(L,U,gmax,NULL,NULL,dim,&box_rg);
	make_level_surface(&box_rg,front->interf,neg_comp,pos_comp,
			ellipsoid_func,(POINTER)&ellip_params,surf);
	wave_type(*surf) = w_type;
	front->interf->modified = YES;
}	/* end FT_MakeEllipticSurf */


EXPORT	void FT_MakeDumbBellSurf(
	Front *front,
	double x0,
	double x1,
	double y0,
	double z0,
	double R,
	double r,
	COMPONENT   neg_comp,
        COMPONENT   pos_comp,
	int w_type,
	SURFACE **surf)
{
	RECT_GRID *rgr = front->rect_grid;
	DUMBBELL_PARAMS d_params;
	int i,dim = rgr->dim;
	double coords[MAXD];

	d_params.x0 = x0;
	d_params.x1 = x1;
	d_params.y = y0;
	d_params.z = z0;
	d_params.R = R;
	d_params.rr = r;
	make_level_surface(rgr,front->interf,neg_comp,pos_comp,
			dumbbell_func,(POINTER)&d_params,surf);
	wave_type(*surf) = w_type;
	front->interf->modified = YES;
}	/* end FT_MakeDumbBellSurf */

EXPORT	void FT_MakeProjectileSurf(
	Front *front,
	double *center,
	double R,
	double r,
	double h,
	COMPONENT   neg_comp,
        COMPONENT   pos_comp,
	int w_type,
	SURFACE **surf)
{
	RECT_GRID *rgr = front->rect_grid;
	PROJECTILE_PARAMS proj_params;
	int i,dim = rgr->dim;
	double coords[MAXD];

	proj_params.dim = dim;
	for (i = 0; i < dim; ++i)
	    proj_params.cen[i] = center[i];
	proj_params.R = R;
	proj_params.r = r;
	proj_params.h = h;
	make_level_surface(rgr,front->interf,neg_comp,pos_comp,
			projectile_func,(POINTER)&proj_params,surf);
	wave_type(*surf) = w_type;
	front->interf->modified = YES;
}	/* end FT_MakeProjectileSurf */

EXPORT	void FT_RotateSurface(
	SURFACE *surf,
	double *center,
	double phi,
	double theta)
{
	POINT *p;
	TRI *tri;
	int i;
	boolean first = YES;

	surf_tri_loop(surf,tri)
	{
	    for (i = 0; i < 3; ++i)
	    {
		p = Point_of_tri(tri)[i];
		sorted(p) = NO;
	    }
	}
	surf_tri_loop(surf,tri)
	{
	    for (i = 0; i < 3; ++i)
	    {
		p = Point_of_tri(tri)[i];
		if (sorted(p)) continue;
		sorted(p) = YES;
		rotate_point_with_polar_angle(p,center,phi,theta,first);
		first = NO;
	    }
	}
}	/* end FT_RotateSurface */


EXPORT  void FT_MakeCuboidSurf(
	Front *front,
	double *center,
	double *edge,
	COMPONENT neg_comp,
	COMPONENT pos_comp,
	int w_type,
	SURFACE **surf)
{
	RECT_GRID *rgr = front->rect_grid;
	CUBOID_PARAMS cuboid_params;
	int i,dim = rgr->dim;
	
	for (i = 0; i < dim; ++i)
	{
	    cuboid_params.center[i] = center[i];
	    cuboid_params.edge[i] = edge[i];
	}
	make_level_surface(rgr,front->interf,neg_comp,pos_comp,
			cuboid_func,(POINTER)&cuboid_params,surf);
        wave_type(*surf) = w_type;
        front->interf->modified = YES;

}	 /*end FT_MakeCuboidSurf*/

EXPORT  void FT_MakeCylinderSurf(
        Front *front,
        double *center,
        double radius,
	double height,
        COMPONENT neg_comp,
        COMPONENT pos_comp,
        int w_type,
        SURFACE **surf)
{
        RECT_GRID *rgr = front->rect_grid;
        CYLINDER_PARAMS cylinder_params;
        int i,dim = rgr->dim;

        for (i = 0; i < dim; ++i)
        {
            cylinder_params.center[i] = center[i];
            cylinder_params.radius = radius;
	    cylinder_params.height = height;	   
        }
        make_level_surface(rgr,front->interf,neg_comp,pos_comp,
                        cylinder_func,(POINTER)&cylinder_params,surf);
        wave_type(*surf) = w_type;
        front->interf->modified = YES;

}        /*end FT_MakeCylinderSurf*/

EXPORT  void FT_MakeConeSurf(
        Front *front,
        double *center,
        double slope,
	double height,
        COMPONENT neg_comp,
        COMPONENT pos_comp,
        int w_type,
        SURFACE **surf)
{
        RECT_GRID *rgr = front->rect_grid;
        CONE_PARAMS cone_params;
        int i,dim = rgr->dim;

        for (i = 0; i < dim; ++i)
        {
            cone_params.center[i] = center[i];
            cone_params.slope = slope;
	    cone_params.height = height;
        }
        make_level_surface(rgr,front->interf,neg_comp,pos_comp,
                        cone_func,(POINTER)&cone_params,surf);
        wave_type(*surf) = w_type;
        front->interf->modified = YES;

}        /*end FT_MakeConeSurf*/

EXPORT  void FT_MakeTetrahedronSurf(
        Front *front,
        double *center,
        double edge,
        COMPONENT neg_comp,
        COMPONENT pos_comp,
        int w_type,
        SURFACE **surf)
{
        RECT_GRID *rgr = front->rect_grid;
        TETRAHEDRON_PARAMS tetrahedron_params;
        int i,dim = rgr->dim;

        for (i = 0; i < dim; ++i)
        {
            tetrahedron_params.center[i] = center[i];
            tetrahedron_params.edge = edge;
        }
        make_level_surface(rgr,front->interf,neg_comp,pos_comp,
                        tetrahedron_func,(POINTER)&tetrahedron_params,surf);
        wave_type(*surf) = w_type;
        front->interf->modified = YES;

}        /*end FT_MakeTetrahedronSurf*/

EXPORT	void FT_MakePlaneSurf(
	Front *front,
	double *plane_nor,
	double *plane_pt,
	boolean reset_bdry_comp,
	COMPONENT   neg_comp,
        COMPONENT   pos_comp,
	int w_type,
	SURFACE **surf)
{
	RECT_GRID *rgr = front->rect_grid;
	PLANE_PARAMS plane_params;
	int i,dim = rgr->dim;

	for (i = 0; i < dim; ++i)
	{
	    plane_params.N[i] = plane_nor[i];
	    plane_params.P[i] = plane_pt[i];
	}
	make_level_surface(rgr,front->interf,neg_comp,pos_comp,
			plane_func,(POINTER)&plane_params,surf);
	wave_type(*surf) = w_type;
	if (reset_bdry_comp)
	{
	    printf("FT_MakePlaneSurf(): reset_bdry_comp code needed\n");
	    clean_up(ERROR);
	}
	front->interf->modified = YES;
}	/* end FT_MakePlaneSurf */

EXPORT	void FT_InstallSurfEdge(
	SURFACE *surf,
	int hsbdry_type)
{
	install_hsbdry_on_surface(surf,hsbdry_type);
}	/* end FT_InstallSurfEdge */

EXPORT  void FT_CutSurfBdry(
        SURFACE *surf,
        boolean constr_func(POINTER,double*),
        POINTER func_params,
        double **insert_coords,
        int num_pts,
        int insert_idir)
{
        int i;
        for (i = 0; i < num_pts; ++i)
        {
            insert_point_in_surface(insert_idir,insert_coords[i],surf);
        }
        cut_surface(surf,constr_func,func_params,YES);
}       /* end FT_CutSurfBdry */

EXPORT	void FT_MakeEllipticCurve(
	Front *front,
	double *center,
	double *radius,
	COMPONENT   neg_comp,
        COMPONENT   pos_comp,
	int w_type,
	int refinement_level,
	CURVE **curve)
{
	ELLIP2D_PARAMS ellip_params;
	int i,num_seg;
	CURVE **curves;
	RECT_GRID box_rg;
	RECT_GRID *rgr = front->rect_grid;
	int dim = rgr->dim;
        double L[MAXD],U[MAXD],dh;
        int gmax[MAXD];
        double *h = rgr->h;

	ellip_params.x0 = center[0];
	ellip_params.y0 = center[1];
	ellip_params.a = radius[0];
	ellip_params.b = radius[1];
	for (i = 0; i < dim; ++i)
        {
            L[i] = center[i] - radius[i];
            U[i] = center[i] + radius[i];
            gmax[i] = rint((U[i] - L[i])/h[i]);
            dh = gmax[i]*h[i] - (U[i] - L[i]);
            L[i] -= 0.5*dh;
            U[i] += 0.5*dh;
            gmax[i] = refinement_level*rint((U[i] - L[i])/h[i]);
        }
	set_box_rect_grid(L,U,gmax,NULL,NULL,dim,&box_rg);
	curves = make_level_curves(&box_rg,front->interf,neg_comp,
			pos_comp,ellipse_func,(POINTER)&ellip_params,
			YES,&num_seg);
	for (i = 0; i < num_seg; ++i)
	    wave_type(curves[i]) = w_type;
	*curve = curves[0];
}	/* end FT_MakeEllipticCurve */
