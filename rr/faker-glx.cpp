/* Copyright (C)2004 Landmark Graphics
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#include "faker-sym.h"
#include "faker-macros.h"

// Map a client-side drawable to a server-side drawable

GLXDrawable ServerDrawable(Display *dpy, GLXDrawable draw)
{
	pbwin *pb=NULL;
	if((pb=winh.findpb(dpy, draw))!=NULL) return pb->getdrawable();
	else return draw;
}

static VisualID _MatchVisual(Display *dpy, GLXFBConfig config)
{
	VisualID vid=0;
	if(!dpy || !config) return 0;
	int screen=DefaultScreen(dpy);
	if(!(vid=cfgh.getvisual(dpy, config)))
	{
		vid=__vglMatchVisual(dpy, screen, __vglConfigDepth(config),
				__vglConfigClass(config),
				__vglServerVisualAttrib(config, GLX_LEVEL),
				__vglServerVisualAttrib(config, GLX_STEREO),
				__vglServerVisualAttrib(config, GLX_TRANSPARENT_TYPE)!=GLX_NONE);
		if(!vid) 
			vid=__vglMatchVisual(dpy, screen, 24, TrueColor, 0, 0, 0);
	}
	if(vid) cfgh.add(dpy, config, vid);
	return vid;
}

static GLXFBConfig _MatchConfig(Display *dpy, XVisualInfo *vis)
{
	GLXFBConfig c=0, *configs=NULL;  int n=0;
	if(!dpy || !vis) return 0;
	if(!(c=vish.getpbconfig(dpy, vis)))
	{
		// Punt.  We can't figure out where the visual came from
		int attribs[]={GLX_DOUBLEBUFFER, 1, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
			GLX_BLUE_SIZE, 8, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE,
			GLX_PBUFFER_BIT, GLX_STEREO, 0, None};
		if(__vglClientVisualAttrib(dpy, DefaultScreen(dpy), vis->visualid, GLX_STEREO))
			attribs[13]=1;
		#ifdef USEGLP
		if(fconfig.glp)
		{
			attribs[10]=attribs[11]=None;
			if((configs=glPChooseFBConfig(_localdev, attribs, &n))==NULL || n<1)
				return 0;
		}
		else
		#endif
		if((configs=_glXChooseFBConfig(_localdpy, vis->screen, attribs, &n))==NULL
			|| n<1)
			return 0;
		c=configs[0];
		XFree(configs);
		if(c)
		{
			vish.add(dpy, vis, c);
			cfgh.add(dpy, c, vis->visualid);
		}
	}
	return c;
}

extern "C" {

GLXFBConfig *glXChooseFBConfig(Display *dpy, int screen, const int *attrib_list, int *nelements)
{
	GLXFBConfig *configs=NULL;
	TRY();

	// Prevent recursion
	if(!_isremote(dpy)) return _glXChooseFBConfig(dpy, screen, attrib_list, nelements);
	////////////////////

	int depth=24, c_class=TrueColor, level=0, stereo=0, trans=0;
	if(!attrib_list || !nelements) return NULL;
	*nelements=0;
	configs=__vglConfigsFromVisAttribs(attrib_list, screen, depth, c_class, level,
		stereo, trans, *nelements, true);
	if(configs && *nelements)
	{
		VisualID vid=__vglMatchVisual(dpy, screen, depth, c_class, level, stereo,
			trans);
		if(vid) for(int i=0; i<*nelements; i++) cfgh.add(dpy, configs[i], vid);
		else {XFree(configs);  return NULL;}
	}

	CATCH();
	return configs;
}

GLXFBConfigSGIX *glXChooseFBConfigSGIX (Display *dpy, int screen, const int *attrib_list, int *nelements)
{
	return glXChooseFBConfig(dpy, screen, attrib_list, nelements);
}

void glXCopyContext(Display *dpy, GLXContext src, GLXContext dst, unsigned int mask)
{
	#ifdef USEGLP
	if(fconfig.glp) glPCopyContext(src, dst, mask);
	else
	#endif
	_glXCopyContext(_localdpy, src, dst, mask);
}

GLXPbuffer glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list)
{
	GLXPbuffer pb=0;
	#ifdef USEGLP
	if(fconfig.glp) pb=glPCreateBuffer(config, attrib_list);
	else
	#endif
	pb=_glXCreatePbuffer(_localdpy, config, attrib_list);
	TRY();
	if(dpy && pb) glxdh.add(pb, dpy);
	CATCH();
	return pb;
}

GLXPbuffer glXCreateGLXPbufferSGIX(Display *dpy, GLXFBConfigSGIX config, unsigned int width, unsigned int height, int *attrib_list)
{
	GLXPbuffer pb=0;
	#ifdef USEGLP
	if(fconfig.glp)
	{
		int glpattribs[257], j=0;
		for(int i=0; attrib_list[i]!=None && i<=254; i+=2)
		{
			glpattribs[j++]=attrib_list[i];  glpattribs[j++]=attrib_list[i+1];
		}
		glpattribs[j++]=GLP_PBUFFER_WIDTH;  glpattribs[j++]=width;
		glpattribs[j++]=GLP_PBUFFER_HEIGHT;  glpattribs[j++]=height;
		glpattribs[j]=None;
		pb=glPCreateBuffer(config, glpattribs);
	}
	#endif
	pb=_glXCreateGLXPbufferSGIX(_localdpy, config, width, height, attrib_list);
	TRY();
	if(dpy && pb) glxdh.add(pb, dpy);
	CATCH();
	return pb;
}

void glXDestroyGLXPbufferSGIX(Display *dpy, GLXPbuffer pbuf)
{
	#ifdef USEGLP
	if(fconfig.glp) glPDestroyBuffer(pbuf);
	else
	#endif
	_glXDestroyGLXPbufferSGIX(_localdpy, pbuf);
	TRY();
	if(pbuf) glxdh.remove(pbuf);
	CATCH();
}

void glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf)
{
	#ifdef USEGLP
	if(fconfig.glp) glPDestroyBuffer(pbuf);
	else
	#endif
	_glXDestroyPbuffer(_localdpy, pbuf);
	TRY();
	if(pbuf) glxdh.remove(pbuf);
	CATCH();
}

void glXFreeContextEXT(Display *dpy, GLXContext ctx)
{
	#ifdef USEGLP
	// Tell me about the rabbits, George ...
	if(fconfig.glp) return;
	else
	#endif
	_glXFreeContextEXT(_localdpy, ctx);
}

const char *glXGetClientString(Display *dpy, int name)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPGetLibraryString(name);
	else
	#endif
	return _glXGetClientString(_localdpy, name);
}

int glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value)
{
	GLXFBConfig c;
	TRY();

	// Prevent recursion
	if(!_isremote(dpy)) return _glXGetConfig(dpy, vis, attrib, value);
	////////////////////

	if(!dpy || !value) throw rrerror("glXGetConfig", "Invalid argument");
	errifnot(c=_MatchConfig(dpy, vis));

	if(attrib==GLX_USE_GL)
	{
		if(vis->c_class==TrueColor || vis->c_class==PseudoColor) *value=1;
		else *value=0;
		return 0;
	}
	if(vis->c_class==PseudoColor && (attrib==GLX_RED_SIZE || attrib==GLX_GREEN_SIZE
		|| attrib==GLX_BLUE_SIZE || attrib==GLX_ALPHA_SIZE
		|| attrib==GLX_ACCUM_RED_SIZE || attrib==GLX_ACCUM_GREEN_SIZE
		|| attrib==GLX_ACCUM_BLUE_SIZE || attrib==GLX_ACCUM_ALPHA_SIZE))
	{
		*value=0;  return 0;
	}
	if(attrib==GLX_BUFFER_SIZE && vis->c_class==PseudoColor)
		attrib=GLX_RED_SIZE;
	if(attrib==GLX_LEVEL || attrib==GLX_TRANSPARENT_TYPE
		|| attrib==GLX_TRANSPARENT_INDEX_VALUE
		|| attrib==GLX_TRANSPARENT_RED_VALUE
		|| attrib==GLX_TRANSPARENT_GREEN_VALUE
		|| attrib==GLX_TRANSPARENT_BLUE_VALUE
		|| attrib==GLX_TRANSPARENT_ALPHA_VALUE)
	{
		*value=__vglClientVisualAttrib(dpy, vis->screen, vis->visualid, attrib);
		return 0;
	}

	if(attrib==GLX_RGBA)
	{
		int render_type=__vglServerVisualAttrib(c, GLX_RENDER_TYPE);
		*value=(render_type==GLX_RGBA_BIT? 1:0);  return 0;
	}
	if(attrib==GLX_STEREO)
	{
		*value= (__vglClientVisualAttrib(dpy, vis->screen, vis->visualid, GLX_STEREO)
			&& __vglServerVisualAttrib(c, GLX_STEREO));  return 0;
	}
	if(attrib==GLX_X_VISUAL_TYPE)
	{
		if(vis->c_class==PseudoColor) *value=GLX_PSEUDO_COLOR;
		else *value=GLX_TRUE_COLOR;
		return 0;
	}

	#ifdef USEGLP
	if(fconfig.glp)
	{
		if(attrib==GLX_VIDEO_RESIZE_SUN) {*value=0;  return 0;}
		else if(attrib==GLX_VIDEO_REFRESH_TIME_SUN) {*value=0;  return 0;}
		else if(attrib==GLX_GAMMA_VALUE_SUN) {*value=100;  return 0;}
		return glPGetFBConfigAttrib(c, attrib, value);
	}
	else
	#endif
	return _glXGetFBConfigAttrib(_localdpy, c, attrib, value);

	CATCH();
	return GLX_BAD_ATTRIBUTE;
}

#ifdef USEGLP
GLXContext glXGetCurrentContext(void)
{
	if(fconfig.glp) return glPGetCurrentContext();
	else
	return _glXGetCurrentContext();
}
#endif

Display *glXGetCurrentDisplay(void)
{
	Display *dpy=NULL;  pbwin *pb=NULL;
	TRY();
	if((pb=winh.findpb(GetCurrentDrawable()))!=NULL)
		dpy=pb->getwindpy();
	else dpy=glxdh.getcurrentdpy(GetCurrentDrawable());
	CATCH();
	return dpy;
}

GLXDrawable glXGetCurrentDrawable(void)
{
	pbwin *pb=NULL;  GLXDrawable draw=GetCurrentDrawable();
	TRY();
	if((pb=winh.findpb(draw))!=NULL)
		draw=pb->getwin();
	CATCH();
	return draw;
}

GLXDrawable glXGetCurrentReadDrawable(void)
{
	pbwin *pb=NULL;  GLXDrawable read=GetCurrentReadDrawable();
	TRY();
	if((pb=winh.findpb(read))!=NULL)
		read=pb->getwin();
	CATCH();
	return read;
}

int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value)
{
	VisualID vid=0;
	TRY();

	// Prevent recursion
	if(!_isremote(dpy)) return _glXGetFBConfigAttrib(dpy, config, attribute, value);
	////////////////////

	if(!dpy || !value) throw rrerror("glXGetFBConfigAttrib", "Invalid argument");
	int screen=DefaultScreen(dpy);

	if(!(vid=_MatchVisual(dpy, config)))
		throw rrerror("glXGetFBConfigAttrib", "Invalid FB config");

	int c_class=__vglVisualClass(dpy, screen, vid);
	if(c_class==PseudoColor && (attribute==GLX_RED_SIZE || attribute==GLX_GREEN_SIZE
		|| attribute==GLX_BLUE_SIZE || attribute==GLX_ALPHA_SIZE
		|| attribute==GLX_ACCUM_RED_SIZE || attribute==GLX_ACCUM_GREEN_SIZE
		|| attribute==GLX_ACCUM_BLUE_SIZE || attribute==GLX_ACCUM_ALPHA_SIZE))
	{
		*value=0;  return 0;
	}
	if(attribute==GLX_BUFFER_SIZE && c_class==PseudoColor)
		attribute=GLX_RED_SIZE;
	if(attribute==GLX_LEVEL || attribute==GLX_TRANSPARENT_TYPE
		|| attribute==GLX_TRANSPARENT_INDEX_VALUE
		|| attribute==GLX_TRANSPARENT_RED_VALUE
		|| attribute==GLX_TRANSPARENT_GREEN_VALUE
		|| attribute==GLX_TRANSPARENT_BLUE_VALUE
		|| attribute==GLX_TRANSPARENT_ALPHA_VALUE)
	{
		*value=__vglClientVisualAttrib(dpy, screen, vid, attribute);  return 0;
	}

	if(attribute==GLX_STEREO)
	{
		*value= (__vglClientVisualAttrib(dpy, screen, vid, GLX_STEREO)
			&& __vglServerVisualAttrib(config, GLX_STEREO));  return 0;
	}
	if(attribute==GLX_X_VISUAL_TYPE)
	{
		if(c_class==PseudoColor) *value=GLX_PSEUDO_COLOR;
		else *value=GLX_TRUE_COLOR;
		return 0;
	}

	#ifdef USEGLP
	if(fconfig.glp)
	{
		if(attribute==GLX_VIDEO_RESIZE_SUN) {*value=0;  return 0;}
		else if(attribute==GLX_VIDEO_REFRESH_TIME_SUN) {*value=0;  return 0;}
		else if(attribute==GLX_GAMMA_VALUE_SUN) {*value=100;  return 0;}
		else return glPGetFBConfigAttrib(config, attribute, value);
	}
	else
	#endif
	return _glXGetFBConfigAttrib(_localdpy, config, attribute, value);

	CATCH();
	return GLX_BAD_ATTRIBUTE;
}

int glXGetFBConfigAttribSGIX(Display *dpy, GLXFBConfigSGIX config, int attribute, int *value_return)
{
	return glXGetFBConfigAttrib(dpy, config, attribute, value_return);
}

GLXFBConfigSGIX glXGetFBConfigFromVisualSGIX(Display *dpy, XVisualInfo *vis)
{
	return _MatchConfig(dpy, vis);
}

GLXFBConfig *glXGetFBConfigs(Display *dpy, int screen, int *nelements)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPGetFBConfigs(_localdev, nelements);
	else
	#endif
	return _glXGetFBConfigs(_localdpy, screen, nelements);
}

void glXGetSelectedEvent(Display *dpy, GLXDrawable draw, unsigned long *event_mask)
{
	#ifdef USEGLP
	if(fconfig.glp) return;
	else
	#endif
	_glXGetSelectedEvent(_localdpy, ServerDrawable(dpy, draw), event_mask);
}

void glXGetSelectedEventSGIX(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
	#ifdef USEGLP
	if(fconfig.glp) return;
	else
	#endif
	_glXGetSelectedEventSGIX(_localdpy, ServerDrawable(dpy, drawable), mask);
}

GLXContext glXImportContextEXT(Display *dpy, GLXContextID contextID)
{
	#ifdef USEGLP
	if(fconfig.glp) return 0;
	else
	#endif
	return _glXImportContextEXT(_localdpy, contextID);
}

Bool glXIsDirect (Display *dpy, GLXContext ctx)
{
	#ifdef USEGLP
	if(fconfig.glp) return True;
	else
	#endif
	return _glXIsDirect(_localdpy, ctx);
}

int glXQueryContext(Display *dpy, GLXContext ctx, int attribute, int *value)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPQueryContext(ctx, attribute, value);
	else
	#endif
	return _glXQueryContext(_localdpy, ctx, attribute, value);
}

int glXQueryContextInfoEXT(Display *dpy, GLXContext ctx, int attribute, int *value)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPQueryContext(ctx, attribute, value);
	else
	#endif
	return _glXQueryContextInfoEXT(_localdpy, ctx, attribute, value);
}

void glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPQueryBuffer(ServerDrawable(dpy, draw), attribute, value);
	else
	#endif
	return _glXQueryDrawable(_localdpy, ServerDrawable(dpy, draw), attribute, value);
}

Bool glXQueryExtension(Display *dpy, int *error_base, int *event_base)
{
	#ifdef USEGLP
	if(fconfig.glp)
		{if(error_base) *error_base=0;  if(event_base) *event_base=0;  return True;}
	else
	#endif
	return _glXQueryExtension(_localdpy, error_base, event_base);
}

const char *glXQueryExtensionsString(Display *dpy, int screen)
{
	#ifdef USEGLP
	if(fconfig.glp) return "GLX_SGIX_fbconfig GLX_SGIX_pbuffer GLX_EXT_visual_info";
	else
	#endif
	return _glXQueryExtensionsString(_localdpy, screen);
}

const char *glXQueryServerString(Display *dpy, int screen, int name)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPGetDeviceString(_localdev, name);
	else
	#endif
	return _glXQueryServerString(_localdpy, screen, name);
}

void glXQueryGLXPbufferSGIX(Display *dpy, GLXPbuffer pbuf, int attribute, unsigned int *value)
{
	#ifdef USEGLP
	if(fconfig.glp) glPQueryBuffer(pbuf, attribute, value);
	else
	#endif
	_glXQueryGLXPbufferSGIX(_localdpy, pbuf, attribute, value);
}

Bool glXQueryVersion(Display *dpy, int *major, int *minor)
{
	#ifdef USEGLP
	if(fconfig.glp) return glPQueryVersion(major, minor);
	else
	#endif
	return _glXQueryVersion(_localdpy, major, minor);
}

void glXSelectEvent(Display *dpy, GLXDrawable draw, unsigned long event_mask)
{
	#ifdef USEGLP
	if(fconfig.glp) return;
	else
	#endif
	_glXSelectEvent(_localdpy, ServerDrawable(dpy, draw), event_mask);
}

void glXSelectEventSGIX(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
	#ifdef USEGLP
	if(fconfig.glp) return;
	else
	#endif
	_glXSelectEventSGIX(_localdpy, ServerDrawable(dpy, drawable), mask);
}

#ifdef sun
int glXDisableXineramaSUN(Display *dpy)
{
	#ifdef USEGLP
	if(fconfig.glp) return 0;
	else
	#endif
	return _glXDisableXineramaSUN(_localdpy);
}
#endif

GLboolean glXGetTransparentIndexSUN(Display *dpy, Window overlay,
	Window underlay, unsigned int *transparentIndex)
{
	XWindowAttributes xwa;
	if(!transparentIndex) return False;
	if(fconfig.transpixel>=0)
	{
		*transparentIndex=(unsigned int)fconfig.transpixel;  return True;
	}
	if(!dpy || !overlay) return False;
	XGetWindowAttributes(dpy, overlay, &xwa);
	*transparentIndex=__vglClientVisualAttrib(dpy, DefaultScreen(dpy),
		xwa.visual->visualid, GLX_TRANSPARENT_INDEX_VALUE);
	return True;
}

Bool glXJoinSwapGroupNV(Display *dpy, GLXDrawable drawable, GLuint group)
{
	return _glXJoinSwapGroupNV(_localdpy, ServerDrawable(dpy, drawable), group);
}

shimfuncdpy3(Bool, glXBindSwapBarrierNV, Display*, dpy, GLuint, group, GLuint, barrier, return );

Bool glXQuerySwapGroupNV(Display *dpy, GLXDrawable drawable, GLuint *group, GLuint *barrier)
{
	return _glXQuerySwapGroupNV(_localdpy, ServerDrawable(dpy, drawable), group, barrier);
}

shimfuncdpy4(Bool, glXQueryMaxSwapGroupsNV, Display*, dpy, int, screen, GLuint*, maxGroups, GLuint*, maxBarriers, return );
shimfuncdpy3(Bool, glXQueryFrameCountNV, Display*, dpy, int, screen, GLuint*, count, return );
shimfuncdpy2(Bool, glXResetFrameCountNV, Display*, dpy, int, screen, return );

}
