#include "Renderer.hpp"
#include "wipemalloc.h"
#include "math.h"
#include "Common.hpp"
#include "CustomShape.hpp"
#include "CustomWave.hpp"
#include "KeyHandler.hpp"
#include "TextureManager.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>
#include "omptl/omptl"
#include "omptl/omptl_algorithm"


class Preset;

Renderer::Renderer(int width, int height, int gx, int gy, int texsize, BeatDetect *beatDetect, std::string _presetURL, std::string _titlefontURL, std::string _menufontURL): title_fontURL(_titlefontURL), menu_fontURL(_menufontURL), presetURL(_presetURL), m_presetName("None"), vw(width), vh(height), texsize(texsize), mesh(gx,gy)

{
	int x; int y;

	this->totalframes = 1;
	this->noSwitch = false;
	this->showfps = false;
	this->showtitle = false;
	this->showpreset = false;
	this->showhelp = false;
	this->showstats = false;
	this->studio = false;
	this->realfps=0;

	this->drawtitle=0;

	//this->title = "Unknown";

	/** Other stuff... */
	this->correction = true;
	this->aspect=1.33333333;

	/// @bug put these on member init list
	this->renderTarget = new RenderTarget( texsize, width, height );
	this->textureManager = new TextureManager(presetURL);
	this->beatDetect = beatDetect;


#ifdef USE_FTGL
	/**f Load the standard fonts */

	title_font = new FTGLPixmapFont(title_fontURL.c_str());
	other_font = new FTGLPixmapFont(menu_fontURL.c_str());
	other_font->UseDisplayList(true);
	title_font->UseDisplayList(true);


	poly_font = new FTGLExtrdFont(title_fontURL.c_str());

	poly_font->UseDisplayList(true);
	poly_font->Depth(20);
	poly_font->FaceSize(72);

	poly_font->UseDisplayList(true);

#endif /** USE_FTGL */

	this->gridx=(float **)wipemalloc(gx * sizeof(float *));
		for(x = 0; x < gx; x++)
		{
			this->gridx[x] = (float *)wipemalloc(gy * sizeof(float));
		}
		this->gridy=(float **)wipemalloc(gx * sizeof(float *));
		for(x = 0; x < gx; x++)
		{
			this->gridy[x] = (float *)wipemalloc(gy * sizeof(float));
		}

		this->origx2=(float **)wipemalloc(gx * sizeof(float *));
		for(x = 0; x < gx; x++)
		{
			this->origx2[x] = (float *)wipemalloc(gy * sizeof(float));
		}
		this->origy2=(float **)wipemalloc(gx * sizeof(float *));
		for(x = 0; x < gx; x++)
		{
			this->origy2[x] = (float *)wipemalloc(gy * sizeof(float));
		}

		//initialize reference grid values
		for (x=0;x<gx;x++)
		{
			for(y=0;y<gy;y++)
			{

				float origx=x/(float)(gx-1);
				float origy=-((y/(float)(gy-1))-1);
				this->gridx[x][y]=origx;
				this->gridy[x][y]=origy;
				this->origx2[x][y]=( origx-.5)*2;
				this->origy2[x][y]=( origy-.5)*2;

			}
		}

#ifdef USE_CG
SetupCg();
#endif
}

#ifdef USE_CG
void Renderer::checkForCgError(const char *situation)
{
  CGerror error;
  const char *string = cgGetLastErrorString(&error);

  if (error != CG_NO_ERROR) {
    printf("%%s: %s\n",
      situation, string);
    if (error == CG_COMPILER_ERROR) {
      //printf("%s\n", cgGetLastListing( myCgContext ));
    }
    exit(1);
  }
}

void Renderer::SetupCg()
{

compositeProgram = "composite";
warpProgram = "warp";
shaderFile="/home/pete/test.cg";

myCgContext = cgCreateContext();
  checkForCgError("creating context");
  cgGLSetDebugMode( CG_FALSE );
  cgSetParameterSettingMode(myCgContext, CG_DEFERRED_PARAMETER_SETTING);


  myCgProfile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
  cgGLSetOptimalOptions(myCgProfile);
  checkForCgError("selecting fragment profile");

  myCgWarpProgram =
    cgCreateProgramFromFile(
      myCgContext,                /* Cg runtime context */
      CG_SOURCE,                  /* Program in human-readable form */
      shaderFile.c_str(),  /* Name of file containing program */
      myCgProfile,        /* Profile: OpenGL ARB vertex program */
      warpProgram.c_str(),      /* Entry function name */
      NULL);                      /* No extra compiler options */
  checkForCgError("creating fragment program from file");
  cgGLLoadProgram(myCgWarpProgram);
  checkForCgError("loading fragment program");



  myCgCompositeProgram =
    cgCreateProgramFromFile(
      myCgContext,                /* Cg runtime context */
      CG_SOURCE,                  /* Program in human-readable form */
      shaderFile.c_str(),  /* Name of file containing program */
      myCgProfile,        /* Profile: OpenGL ARB vertex program */
      compositeProgram.c_str(),      /* Entry function name */
      NULL);                      /* No extra compiler options */
  checkForCgError("creating fragment program from file");
  cgGLLoadProgram(myCgCompositeProgram);
  checkForCgError("loading fragment program");

}

void Renderer::SetupCgVariables(CGprogram program, const PipelineContext &context)
{
	cgGLSetParameter1f(cgGetNamedParameter(program, "time"), context.time);
	  cgGLSetParameter4f(cgGetNamedParameter(program, "rand_preset"), 0.6, 0.43, 0.87, 0.3);
	cgGLSetParameter4f(cgGetNamedParameter(program, "rand_frame"), (rand()%100) * .01,(rand()%100) * .01,(rand()%100) * .01,(rand()%100) * .01);
	cgGLSetParameter1f(cgGetNamedParameter(program, "fps"), context.fps);
	cgGLSetParameter1f(cgGetNamedParameter(program, "frame"), context.frame);
	cgGLSetParameter1f(cgGetNamedParameter(program, "progress"), context.progress);

	cgGLSetParameter1f(cgGetNamedParameter(program, "bass"), beatDetect->bass);
	cgGLSetParameter1f(cgGetNamedParameter(program, "mid"), beatDetect->mid);
	cgGLSetParameter1f(cgGetNamedParameter(program, "treb"), beatDetect->treb);
	cgGLSetParameter1f(cgGetNamedParameter(program, "bass_att"), beatDetect->bass_att);
	cgGLSetParameter1f(cgGetNamedParameter(program, "mid_att"), beatDetect->mid_att);
	cgGLSetParameter1f(cgGetNamedParameter(program, "treb_att"), beatDetect->treb_att);
	cgGLSetParameter1f(cgGetNamedParameter(program, "vol"), beatDetect->vol);
	  cgGLSetParameter1f(cgGetNamedParameter(program, "vol_att"), beatDetect->vol);

	cgGLSetParameter4f(cgGetNamedParameter(program, "texsize"), renderTarget->texsize, renderTarget->texsize, 1/(float)renderTarget->texsize,1/(float)renderTarget->texsize);
  	  cgGLSetParameter4f(cgGetNamedParameter(program, "aspect"), aspect,1,1/aspect,1);



}

#endif

void Renderer::ResetTextures()
{
	textureManager->Clear();

	delete(renderTarget);
	renderTarget = new RenderTarget(texsize, vw, vh);
	reset(vw, vh);

	textureManager->Preload();
}

void Renderer::RenderFrame(const Pipeline* pipeline, const PipelineContext &pipelineContext)
{

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	totalframes++;
	renderTarget->lock();
	glViewport( 0, 0, renderTarget->texsize, renderTarget->texsize );



	glEnable( GL_TEXTURE_2D );

	//If using FBO, switch to FBO texture
	if(this->renderTarget->useFBO)
		glBindTexture( GL_TEXTURE_2D, renderTarget->textureID[1] );
	else
		glBindTexture( GL_TEXTURE_2D, renderTarget->textureID[0] );

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();

	#ifdef USE_GLES1
		glOrthof(0.0, 1, 0.0, 1, -40, 40);
	#else
		glOrtho(0.0, 1, 0.0, 1, -40, 40);
	#endif

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();

#ifdef USE_CG
  cgGLBindProgram(myCgWarpProgram);
  checkForCgError("binding warp program");

  cgGLEnableProfile(myCgProfile);
  checkForCgError("enabling warp profile");
#endif
		Interpolation(pipeline);
#ifdef USE_CG
  cgGLUnbindProgram(myCgProfile);
  checkForCgError("disabling fragment profile");
  cgGLDisableProfile(myCgProfile);
  checkForCgError("disabling fragment profile");
#endif

	    renderContext.time = pipelineContext.time;
		renderContext.texsize = texsize;
		renderContext.aspectCorrect = correction;
		renderContext.aspectRatio = aspect;
		renderContext.textureManager = textureManager;
		renderContext.beatDetect = beatDetect;

		for (std::vector<RenderItem*>::const_iterator pos = pipeline->drawables.begin(); pos != pipeline->drawables.end(); ++pos)
			(*pos)->Draw(renderContext);

					/** Restore original view state */
			glMatrixMode( GL_MODELVIEW );
			glPopMatrix();

			glMatrixMode( GL_PROJECTION );
			glPopMatrix();



			renderTarget->unlock();

			//BEGIN PASS 2
			//
			//end of texture rendering
			//now we copy the texture from the FBO or framebuffer to
			//video texture memory and render fullscreen.

			/** Reset the viewport size */
		#ifdef USE_FBO
			if(renderTarget->renderToTexture)
			{
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->renderTarget->fbuffer[1]);
				glViewport( 0, 0, this->renderTarget->texsize, this->renderTarget->texsize );
			}
			else
		#endif
			glViewport( 0, 0, this->vw, this->vh );

			glBindTexture( GL_TEXTURE_2D, this->renderTarget->textureID[0] );

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();

#ifdef USE_GLES1
		glOrthof(-0.5, 0.5, -0.5, 0.5, -40, 40);
#else
		glOrtho(-0.5, 0.5, -0.5, 0.5, -40, 40);
#endif

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glLineWidth( this->renderTarget->texsize < 512 ? 1 : this->renderTarget->texsize/512.0);

			CompositeOutput(pipeline);
		#ifdef USE_FBO
			if(renderTarget->renderToTexture)
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		#endif

}


void Renderer::RenderFrame(PresetOutputs *presetOutputs, PresetInputs *presetInputs)
{
	/** Save original view state */
	// TODO: check there is sufficient room on the stack
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	totalframes++;

	//BEGIN PASS 1
	//
	//This pass is used to render our texture
	//the texture is drawn to a FBO or a subsection of the framebuffer
	//and then we perform our manipulations on it in pass 2 we
	//will copy the image into texture memory and render the final image


	//Lock FBO
	renderTarget->lock();

	glViewport( 0, 0, renderTarget->texsize, renderTarget->texsize );

	glEnable( GL_TEXTURE_2D );

	//If using FBO, sitch to FBO texture
	if(this->renderTarget->useFBO)
	{
		glBindTexture( GL_TEXTURE_2D, renderTarget->textureID[1] );
	}
	else
	{
		glBindTexture( GL_TEXTURE_2D, renderTarget->textureID[0] );
	}

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();


	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
#ifdef USE_GLES1
	glOrthof(0.0, 1, 0.0, 1, -40, 40);
#else
	glOrtho(0.0, 1, 0.0, 1, -40, 40);
#endif
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	renderContext.time = presetInputs->time;
	renderContext.texsize = texsize;
	renderContext.aspectCorrect = correction;
	renderContext.aspectRatio = aspect;
	renderContext.textureManager = textureManager;
	renderContext.beatDetect = beatDetect;

	Interpolation(presetOutputs, presetInputs);


	presetOutputs->mv.Draw(renderContext);



	draw_shapes(presetOutputs);
	draw_custom_waves(presetOutputs);
	presetOutputs->wave.Draw(renderContext);
	if(presetOutputs->bDarkenCenter)darken_center();
	presetOutputs->border.Draw(renderContext);
	draw_title_to_texture();
	/** Restore original view state */
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();

	renderTarget->unlock();


#ifdef DEBUG
	GLint msd = 0, psd = 0;
	glGetIntegerv( GL_MODELVIEW_STACK_DEPTH, &msd );
	glGetIntegerv( GL_PROJECTION_STACK_DEPTH, &psd );
	DWRITE( "end pass1: modelview matrix depth: %d\tprojection matrix depth: %d\n", msd, psd );
	DWRITE( "begin pass2\n" );
#endif

	//BEGIN PASS 2
	//
	//end of texture rendering
	//now we copy the texture from the FBO or framebuffer to
	//video texture memory and render fullscreen.

	/** Reset the viewport size */
#ifdef USE_FBO
	if(renderTarget->renderToTexture)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->renderTarget->fbuffer[1]);
		glViewport( 0, 0, this->renderTarget->texsize, this->renderTarget->texsize );
	}
	else
#endif
	glViewport( 0, 0, this->vw, this->vh );



	glBindTexture( GL_TEXTURE_2D, this->renderTarget->textureID[0] );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
#ifdef USE_GLES1
	glOrthof(-0.5, 0.5, -0.5, 0.5, -40, 40);
#else
	glOrtho(-0.5, 0.5, -0.5, 0.5, -40, 40);
#endif
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth( this->renderTarget->texsize < 512 ? 1 : this->renderTarget->texsize/512.0);

	render_texture_to_screen(presetOutputs);


	glMatrixMode(GL_MODELVIEW);
	glTranslatef(-0.5, -0.5, 0);

	// When console refreshes, there is a chance the preset has been changed by the user
	refreshConsole();
	draw_title_to_screen(false);
	if(this->showhelp%2) draw_help();
	if(this->showtitle%2) draw_title();
	if(this->showfps%2) draw_fps(this->realfps);
	if(this->showpreset%2) draw_preset();
	if(this->showstats%2) draw_stats(presetInputs);
	glTranslatef(0.5 , 0.5, 0);

#ifdef USE_FBO
	if(renderTarget->renderToTexture)
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif
}


void Renderer::Interpolation(PresetOutputs *presetOutputs, PresetInputs *presetInputs)
{
	//Texture wrapping( clamp vs. wrap)
	if (presetOutputs->bTexWrap==0)
	{
#ifdef USE_GLES1
	  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
	}
	else
	{ glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);}

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glBlendFunc(GL_SRC_ALPHA, GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, presetOutputs->decay);

	glEnable(GL_TEXTURE_2D);

	int size = presetInputs->gy;

	float p[size*2][2];
	float t[size*2][2];

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2,GL_FLOAT,0,p);
	glTexCoordPointer(2,GL_FLOAT,0,t);

	for (int x=0;x<presetInputs->gx - 1;x++)
	{
		for(int y=0;y<presetInputs->gy;y++)
		{
		  t[y*2][0] = presetOutputs->x_mesh[x][y];
		  t[y*2][1] = presetOutputs->y_mesh[x][y];

		  p[y*2][0] = this->gridx[x][y];
		  p[y*2][1] = this->gridy[x][y];

		  t[(y*2)+1][0] = presetOutputs->x_mesh[x+1][y];
		  t[(y*2)+1][1] = presetOutputs->y_mesh[x+1][y];

		  p[(y*2)+1][0] = this->gridx[x+1][y];
		  p[(y*2)+1][1] = this->gridy[x+1][y];

		}
	       	glDrawArrays(GL_TRIANGLE_STRIP,0,size*2);
	}



	glDisable(GL_TEXTURE_2D);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}


void Renderer::Interpolation(const Pipeline *pipeline)
{
	//Texture wrapping( clamp vs. wrap)
	if (pipeline->textureWrap==0)
	{
#ifdef USE_GLES1
	  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
	}
	else
	{ glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);}

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glBlendFunc(GL_SRC_ALPHA, GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, pipeline->screenDecay);

	glEnable(GL_TEXTURE_2D);

	int size = mesh.width * 2;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	float p[size][2];
	float t[size][2];

	glVertexPointer(2,GL_FLOAT,0,p);
	glTexCoordPointer(2,GL_FLOAT,0,t);

	mesh.Reset();
	currentPipe = const_cast<Pipeline*>(pipeline);
	omptl::transform(mesh.p.begin(), mesh.p.end(), mesh.identity.begin(), mesh.p.begin(), &Renderer::PerPixel);

	for (int j=0;j<mesh.height - 1;j++)
	{
				for(int i=0;i<mesh.width;i++)
				{
					int index = j * mesh.width + i;
					int index2 = (j+1) * mesh.width + i;

					t[i*2][0] = mesh.p[index].x;
					t[i*2][1] = mesh.p[index].y;
					p[i*2][0] = mesh.identity[index].x;
					p[i*2][1] = mesh.identity[index].y;

					t[(i*2)+1][0] = mesh.p[index2].x;
					t[(i*2)+1][1] = mesh.p[index2].y;
					p[(i*2)+1][0] = mesh.identity[index2].x;
					p[(i*2)+1][1] = mesh.identity[index2].y;
  			  		}
		       	glDrawArrays(GL_TRIANGLE_STRIP,0,size);
	}

	glDisable(GL_TEXTURE_2D);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}
Pipeline* Renderer::currentPipe;


Renderer::~Renderer()
{

	int x;


	if (renderTarget)
		delete(renderTarget);
	if (textureManager)
		delete(textureManager);

//std::cerr << "grid assign end" << std::endl;

	for(x = 0; x < mesh.width; x++)
		{
			free(this->gridx[x]);
			free(this->gridy[x]);
			free(this->origx2[x]);
			free(this->origy2[x]);
		}


		//std::cerr << "freeing grids" << std::endl;
		free(this->origx2);
		free(this->origy2);
		free(this->gridx);
		free(this->gridy);

	//std:cerr << "grid assign begin " << std::endl;
		this->origx2 = NULL;
		this->origy2 = NULL;
		this->gridx = NULL;
		this->gridy = NULL;

#ifdef USE_FTGL
//	std::cerr << "freeing title fonts" << std::endl;
	if (title_font)
		delete title_font;
	if (poly_font)
		delete poly_font;
	if (other_font)
		delete other_font;
//	std::cerr << "freeing title fonts finished" << std::endl;
#endif
//	std::cerr << "exiting destructor" << std::endl;
}


void Renderer::PerPixelMath(PresetOutputs * presetOutputs, PresetInputs * presetInputs)
{

	int x, y;
	float fZoom2, fZoom2Inv;


	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			fZoom2 = powf( presetOutputs->zoom_mesh[x][y], powf( presetOutputs->zoomexp_mesh[x][y], presetInputs->rad_mesh[x][y]*2.0f - 1.0f));
			fZoom2Inv = 1.0f/fZoom2;
			presetOutputs->x_mesh[x][y]= this->origx2[x][y]*0.5f*fZoom2Inv + 0.5f;
			presetOutputs->y_mesh[x][y]= this->origy2[x][y]*0.5f*fZoom2Inv + 0.5f;
		}
	}

	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			presetOutputs->x_mesh[x][y]  = ( presetOutputs->x_mesh[x][y] - presetOutputs->cx_mesh[x][y])/presetOutputs->sx_mesh[x][y] + presetOutputs->cx_mesh[x][y];
		}
	}

	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			presetOutputs->y_mesh[x][y] = ( presetOutputs->y_mesh[x][y] - presetOutputs->cy_mesh[x][y])/presetOutputs->sy_mesh[x][y] + presetOutputs->cy_mesh[x][y];
		}
	}

	float fWarpTime = presetInputs->time * presetOutputs->fWarpAnimSpeed;
	float fWarpScaleInv = 1.0f / presetOutputs->fWarpScale;
	float f[4];
	f[0] = 11.68f + 4.0f*cosf(fWarpTime*1.413f + 10);
	f[1] =  8.77f + 3.0f*cosf(fWarpTime*1.113f + 7);
	f[2] = 10.54f + 3.0f*cosf(fWarpTime*1.233f + 3);
	f[3] = 11.49f + 4.0f*cosf(fWarpTime*0.933f + 5);

	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			presetOutputs->x_mesh[x][y] += presetOutputs->warp_mesh[x][y]*0.0035f*sinf(fWarpTime*0.333f + fWarpScaleInv*(this->origx2[x][y]*f[0] - this->origy2[x][y]*f[3]));
			presetOutputs->y_mesh[x][y] += presetOutputs->warp_mesh[x][y]*0.0035f*cosf(fWarpTime*0.375f - fWarpScaleInv*(this->origx2[x][y]*f[2] + this->origy2[x][y]*f[1]));
			presetOutputs->x_mesh[x][y] += presetOutputs->warp_mesh[x][y]*0.0035f*cosf(fWarpTime*0.753f - fWarpScaleInv*(this->origx2[x][y]*f[1] - this->origy2[x][y]*f[2]));
			presetOutputs->y_mesh[x][y] += presetOutputs->warp_mesh[x][y]*0.0035f*sinf(fWarpTime*0.825f + fWarpScaleInv*(this->origx2[x][y]*f[0] + this->origy2[x][y]*f[3]));
		}
	}
	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			float u2 = presetOutputs->x_mesh[x][y] - presetOutputs->cx_mesh[x][y];
			float v2 = presetOutputs->y_mesh[x][y] - presetOutputs->cy_mesh[x][y];

			float cos_rot = cosf(presetOutputs->rot_mesh[x][y]);
			float sin_rot = sinf(presetOutputs->rot_mesh[x][y]);

			presetOutputs->x_mesh[x][y] = u2*cos_rot - v2*sin_rot + presetOutputs->cx_mesh[x][y];
			presetOutputs->y_mesh[x][y] = u2*sin_rot + v2*cos_rot + presetOutputs->cy_mesh[x][y];

		}
	}


	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			presetOutputs->x_mesh[x][y] -= presetOutputs->dx_mesh[x][y];
		}
	}



	for (x=0;x<mesh.width;x++)
	{
		for(y=0;y<mesh.height;y++)
		{
			presetOutputs->y_mesh[x][y] -= presetOutputs->dy_mesh[x][y];
		}
	}

}




void Renderer::reset(int w, int h)
{
	this->aspect=(float)h / (float)w;
	this -> vw = w;
	this -> vh = h;

	glShadeModel( GL_SMOOTH);

	glCullFace( GL_BACK );
	//glFrontFace( GL_CCW );

	glClearColor( 0, 0, 0, 0 );

	glViewport( 0, 0, w, h );

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

#ifndef USE_GLES1
	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);
#endif
	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable( GL_LINE_SMOOTH );


	glEnable(GL_POINT_SMOOTH);
	glClear(GL_COLOR_BUFFER_BIT);

#ifndef USE_GLES1
	glLineStipple(2, 0xAAAA);
#endif

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,  GL_MODULATE);

	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	//glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	//glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	if (!this->renderTarget->useFBO)
	{
		this->renderTarget->fallbackRescale(w, h);
	}
}


void Renderer::draw_custom_waves(PresetOutputs *presetOutputs)
{

	int x;


	glPointSize(this->renderTarget->texsize < 512 ? 1 : this->renderTarget->texsize/512);

	for (PresetOutputs::cwave_container::const_iterator pos = presetOutputs->customWaves.begin();
	pos != presetOutputs->customWaves.end(); ++pos)
	{

		if( (*pos)->enabled==1)
		{

			if ( (*pos)->bAdditive==0)  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			if ( (*pos)->bDrawThick==1)
			{ glLineWidth(this->renderTarget->texsize < 512 ? 1 : 2*this->renderTarget->texsize/512);
			  glPointSize(this->renderTarget->texsize < 512 ? 1 : 2*this->renderTarget->texsize/512);

			}
			beatDetect->pcm->getPCM( (*pos)->value1, (*pos)->samples, 0, (*pos)->bSpectrum, (*pos)->smoothing, 0);
			beatDetect->pcm->getPCM( (*pos)->value2, (*pos)->samples, 1, (*pos)->bSpectrum, (*pos)->smoothing, 0);
			// printf("%f\n",pcmL[0]);


			float mult= (*pos)->scaling*presetOutputs->wave.scale*( (*pos)->bSpectrum ? 0.015f :1.0f);

			for(x=0;x< (*pos)->samples;x++)
			{ (*pos)->value1[x]*=mult;}

			for(x=0;x< (*pos)->samples;x++)
			{ (*pos)->value2[x]*=mult;}

			for(x=0;x< (*pos)->samples;x++)
			{ (*pos)->sample_mesh[x]=((float)x)/((float)( (*pos)->samples-1));}

			// printf("mid inner loop\n");
			(*pos)->evalPerPointEqns();


			float colors[(*pos)->samples][4];
			float points[(*pos)->samples][2];

			for(x=0;x< (*pos)->samples;x++)
			{
			  colors[x][0] = (*pos)->r_mesh[x];
			  colors[x][1] = (*pos)->g_mesh[x];
			  colors[x][2] = (*pos)->b_mesh[x];
			  colors[x][3] = (*pos)->a_mesh[x];

			  points[x][0] = (*pos)->x_mesh[x];
			  points[x][1] =  -( (*pos)->y_mesh[x]-1);

			}

			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			glVertexPointer(2,GL_FLOAT,0,points);
			glColorPointer(4,GL_FLOAT,0,colors);


			if ( (*pos)->bUseDots==1)
		       	glDrawArrays(GL_POINTS,0,(*pos)->samples);
			else  	glDrawArrays(GL_LINE_STRIP,0,(*pos)->samples);

			glPointSize(this->renderTarget->texsize < 512 ? 1 : this->renderTarget->texsize/512);
			glLineWidth(this->renderTarget->texsize < 512 ? 1 : this->renderTarget->texsize/512);
#ifndef USE_GLES1
			glDisable(GL_LINE_STIPPLE);
#endif
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			//  glPopMatrix();

		}
	}


}

void Renderer::draw_shapes(PresetOutputs *presetOutputs)
{



	float xval, yval;
	float t;

	float aspect=this->aspect;

	for (PresetOutputs::cshape_container::const_iterator pos = presetOutputs->customShapes.begin();
	pos != presetOutputs->customShapes.end(); ++pos)
	{

		if( (*pos)->enabled==1)
		{

			// printf("drawing shape %f\n", (*pos)->ang);
			(*pos)->y=-(( (*pos)->y)-1);

			(*pos)->radius= (*pos)->radius*(.707*.707*.707*1.04);
			//Additive Drawing or Overwrite
			if ( (*pos)->additive==0)  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			xval= (*pos)->x;
			yval= (*pos)->y;

			if ( (*pos)->textured)
			{

				if ((*pos)->getImageUrl() !="")
				{
					GLuint tex = textureManager->getTexture((*pos)->getImageUrl());
					if (tex != 0)
					{
						glBindTexture(GL_TEXTURE_2D, tex);
						aspect=1.0;
					}
				}



				glMatrixMode(GL_TEXTURE);
				glPushMatrix();
				glLoadIdentity();

				glEnable(GL_TEXTURE_2D);

				glEnableClientState(GL_VERTEX_ARRAY);
				glEnableClientState(GL_COLOR_ARRAY);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);

				float colors[(*pos)->sides+2][4];
				float tex[(*pos)->sides+2][2];
				float points[(*pos)->sides+2][2];

				//Define the center point of the shape
				colors[0][0] = (*pos)->r;
				colors[0][1] = (*pos)->g;
				colors[0][2] = (*pos)->b;
				colors[0][3] = (*pos)->a;
			  	   tex[0][0] = 0.5;
				   tex[0][1] = 0.5;
				points[0][0] = xval;
				points[0][1] = yval;

				for ( int i=1;i< (*pos)->sides+2;i++)
				{
				  colors[i][0]= (*pos)->r2;
				  colors[i][1]=(*pos)->g2;
				  colors[i][2]=(*pos)->b2;
				  colors[i][3]=(*pos)->a2;

				  t = (i-1)/(float) (*pos)->sides;
				  tex[i][0] =0.5f + 0.5f*cosf(t*3.1415927f*2 +  (*pos)->tex_ang + 3.1415927f*0.25f)*(this->correction ? aspect : 1.0)/ (*pos)->tex_zoom;
				  tex[i][1] =  0.5f + 0.5f*sinf(t*3.1415927f*2 +  (*pos)->tex_ang + 3.1415927f*0.25f)/ (*pos)->tex_zoom;
				  points[i][0]=(*pos)->radius*cosf(t*3.1415927f*2 +  (*pos)->ang + 3.1415927f*0.25f)*(this->correction ? aspect : 1.0)+xval;
				  points[i][1]=(*pos)->radius*sinf(t*3.1415927f*2 +  (*pos)->ang + 3.1415927f*0.25f)+yval;



				}

				glVertexPointer(2,GL_FLOAT,0,points);
				glColorPointer(4,GL_FLOAT,0,colors);
				glTexCoordPointer(2,GL_FLOAT,0,tex);

				glDrawArrays(GL_TRIANGLE_FAN,0,(*pos)->sides+2);

				glDisable(GL_TEXTURE_2D);
				glPopMatrix();
				glMatrixMode(GL_MODELVIEW);

				//Reset Texture state since we might have changed it
				if(this->renderTarget->useFBO)
				{
					glBindTexture( GL_TEXTURE_2D, renderTarget->textureID[1] );
				}
				else
				{
					glBindTexture( GL_TEXTURE_2D, renderTarget->textureID[0] );
				}


			}
			else
			{//Untextured (use color values)


			  glEnableClientState(GL_VERTEX_ARRAY);
			  glEnableClientState(GL_COLOR_ARRAY);
			  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			  float colors[(*pos)->sides+2][4];
			  float points[(*pos)->sides+2][2];

			  //Define the center point of the shape
			  colors[0][0]=(*pos)->r;
			  colors[0][1]=(*pos)->g;
			  colors[0][2]=(*pos)->b;
			  colors[0][3]=(*pos)->a;
			  points[0][0]=xval;
			  points[0][1]=yval;



			  for ( int i=1;i< (*pos)->sides+2;i++)
			    {
			      colors[i][0]=(*pos)->r2;
			      colors[i][1]=(*pos)->g2;
			      colors[i][2]=(*pos)->b2;
			      colors[i][3]=(*pos)->a2;
			      t = (i-1)/(float) (*pos)->sides;
			      points[i][0]=(*pos)->radius*cosf(t*3.1415927f*2 +  (*pos)->ang + 3.1415927f*0.25f)*(this->correction ? aspect : 1.0)+xval;
			      points[i][1]=(*pos)->radius*sinf(t*3.1415927f*2 +  (*pos)->ang + 3.1415927f*0.25f)+yval;

			    }

			  glVertexPointer(2,GL_FLOAT,0,points);
			  glColorPointer(4,GL_FLOAT,0,colors);


			  glDrawArrays(GL_TRIANGLE_FAN,0,(*pos)->sides+2);
			  //draw first n-1 triangular pieces

			}
			if ((*pos)->thickOutline==1)  glLineWidth(this->renderTarget->texsize < 512 ? 1 : 2*this->renderTarget->texsize/512);

			glEnableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);

			float points[(*pos)->sides+1][2];

			glColor4f( (*pos)->border_r, (*pos)->border_g, (*pos)->border_b, (*pos)->border_a);

			for ( int i=0;i< (*pos)->sides;i++)
			{
				t = (i-1)/(float) (*pos)->sides;
				points[i][0]= (*pos)->radius*cosf(t*3.1415927f*2 +  (*pos)->ang + 3.1415927f*0.25f)*(this->correction ? aspect : 1.0)+xval;
				points[i][1]=  (*pos)->radius*sinf(t*3.1415927f*2 +  (*pos)->ang + 3.1415927f*0.25f)+yval;

			}

			glVertexPointer(2,GL_FLOAT,0,points);
			glDrawArrays(GL_LINE_LOOP,0,(*pos)->sides);

			if ((*pos)->thickOutline==1)  glLineWidth(this->renderTarget->texsize < 512 ? 1 : this->renderTarget->texsize/512);


		}
	}


}


void Renderer::darken_center()
{

	float unit=0.05f;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float colors[6][4] = {{0, 0, 0, 3.0f/32.0f},
			      {0, 0, 0, 0},
			      {0, 0, 0, 0},
			      {0, 0, 0, 0},
			      {0, 0, 0, 0},
			      {0, 0, 0, 0}};

	float points[6][2] = {{ 0.5,  0.5},
			      { 0.45, 0.5},
			      { 0.5,  0.45},
			      { 0.55, 0.5},
			      { 0.5,  0.55},
			      { 0.45, 0.5}};

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2,GL_FLOAT,0,points);
	glColorPointer(4,GL_FLOAT,0,colors);

	glDrawArrays(GL_TRIANGLE_FAN,0,6);

}




GLuint Renderer::initRenderToTexture()
{
	return renderTarget->initRenderToTexture();
}


void Renderer::draw_title_to_texture()
{
#ifdef USE_FTGL
	if (this->drawtitle>100)
	{
		draw_title_to_screen(true);
		this->drawtitle=0;
	}
#endif /** USE_FTGL */
}

/*
void setUpLighting()
{
	// Set up lighting.
	float light1_ambient[4]  = { 1.0, 1.0, 1.0, 1.0 };
	float light1_diffuse[4]  = { 1.0, 0.9, 0.9, 1.0 };
	float light1_specular[4] = { 1.0, 0.7, 0.7, 1.0 };
	float light1_position[4] = { -1.0, 1.0, 1.0, 0.0 };
	glLightfv(GL_LIGHT1, GL_AMBIENT,  light1_ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1_diffuse);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
	glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
	//glEnable(GL_LIGHT1);

	float light2_ambient[4]  = { 0.2, 0.2, 0.2, 1.0 };
	float light2_diffuse[4]  = { 0.9, 0.9, 0.9, 1.0 };
	float light2_specular[4] = { 0.7, 0.7, 0.7, 1.0 };
	float light2_position[4] = { 0.0, -1.0, 1.0, 0.0 };
	glLightfv(GL_LIGHT2, GL_AMBIENT,  light2_ambient);
	glLightfv(GL_LIGHT2, GL_DIFFUSE,  light2_diffuse);
	glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
	glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
	glEnable(GL_LIGHT2);

	float front_emission[4] = { 0.3, 0.2, 0.1, 0.0 };
	float front_ambient[4]  = { 0.2, 0.2, 0.2, 0.0 };
	float front_diffuse[4]  = { 0.95, 0.95, 0.8, 0.0 };
	float front_specular[4] = { 0.6, 0.6, 0.6, 0.0 };
	glMaterialfv(GL_FRONT, GL_EMISSION, front_emission);
	glMaterialfv(GL_FRONT, GL_AMBIENT, front_ambient);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, front_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, front_specular);
	glMaterialf(GL_FRONT, GL_SHININESS, 16.0);
	glColor4fv(front_diffuse);

	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	glEnable(GL_LIGHTING);
}
*/

float title_y;

void Renderer::draw_title_to_screen(bool flip)
{

#ifdef USE_FTGL
	if(this->drawtitle>0)
	{

	  //setUpLighting();

		//glEnable(GL_POLYGON_SMOOTH);
		//glEnable( GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_DEPTH_BUFFER_BIT);

		int draw;
		if (drawtitle>=80) draw = 80;
		else draw = drawtitle;

		float easein = ((80-draw)*.0125);
		float easein2 = easein * easein;

		if(drawtitle==1)
		{
			title_y = (float)rand()/RAND_MAX;
			title_y *= 2;
			title_y -= 1;
			title_y *= .6;
		}
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glBlendFunc(GL_SRC_ALPHA_SATURATE,GL_ONE);
		glColor4f(1.0, 1.0, 1.0, 1.0);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();

		glFrustum(-1, 1, -1 * (float)vh/(float)vw, 1 *(float)vh/(float)vw, 1, 1000);
		if (flip) glScalef(1, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glTranslatef(-850, title_y * 850 *vh/vw  , easein2*900-900);

		glRotatef(easein2*360, 1, 0, 0);

		poly_font->Render(this->title.c_str() );

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		this->drawtitle++;

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();

		glMatrixMode(GL_MODELVIEW);

		glDisable( GL_CULL_FACE);
		glDisable( GL_DEPTH_TEST);

		glDisable(GL_COLOR_MATERIAL);

		glDisable(GL_LIGHTING);
		glDisable(GL_POLYGON_SMOOTH);
	}
#endif /** USE_FTGL */
}



void Renderer::draw_title()
{
#ifdef USE_FTGL
	//glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, 1.0);
	//  glPushMatrix();
	//  glTranslatef(this->vw*.001,this->vh*.03, -1);
	//  glScalef(this->vw*.015,this->vh*.025,0);

	glRasterPos2f(0.01, 0.05);
	title_font->FaceSize( (unsigned)(20*(this->vh/512.0)));


	title_font->Render(this->title.c_str() );
	//  glPopMatrix();
	//glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

#endif /** USE_FTGL */
}

void Renderer::draw_preset()
{
#ifdef USE_FTGL
	//glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, 1.0);
	//      glPushMatrix();
	//glTranslatef(this->vw*.001,this->vh*-.01, -1);
	//glScalef(this->vw*.003,this->vh*.004,0);


	glRasterPos2f(0.01, 0.01);

	title_font->FaceSize((unsigned)(12*(this->vh/512.0)));
	if(this->noSwitch) title_font->Render("[LOCKED]  " );
	title_font->FaceSize((unsigned)(20*(this->vh/512.0)));

	title_font->Render(this->presetName().c_str() );



	//glPopMatrix();
	// glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
#endif /** USE_FTGL */
}

void Renderer::draw_help( )
{

#ifdef USE_FTGL
//glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, 1.0);
	glPushMatrix();
	glTranslatef(0, 1, 0);
	//glScalef(this->vw*.02,this->vh*.02 ,0);


	title_font->FaceSize((unsigned)( 18*(this->vh/512.0)));

	glRasterPos2f(0.01, -0.05);
	title_font->Render("Help");

	glRasterPos2f(0.01, -0.09);
	title_font->Render("----------------------------");

	glRasterPos2f(0.01, -0.13);
	title_font->Render("F1: This help menu");

	glRasterPos2f(0.01, -0.17);
	title_font->Render("F2: Show song title");

	glRasterPos2f(0.01, -0.21);
	title_font->Render("F3: Show preset name");

	glRasterPos2f(0.01, -0.25);
	title_font->Render("F4: Show Rendering Settings");

	glRasterPos2f(0.01, -0.29);
	title_font->Render("F5: Show FPS");

	glRasterPos2f(0.01, -0.35);
	title_font->Render("F: Fullscreen");

	glRasterPos2f(0.01, -0.39);
	title_font->Render("L: Lock/Unlock Preset");

	glRasterPos2f(0.01, -0.43);
	title_font->Render("M: Show Menu");

	glRasterPos2f(0.01, -0.49);
	title_font->Render("R: Random preset");
	glRasterPos2f(0.01, -0.53);
	title_font->Render("N: Next preset");

	glRasterPos2f(0.01, -0.57);
	title_font->Render("P: Previous preset");

	glPopMatrix();
	//         glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

#endif /** USE_FTGL */
}

void Renderer::draw_stats(PresetInputs *presetInputs)
{

#ifdef USE_FTGL
	char buffer[128];
	float offset= (this->showfps%2 ? -0.05 : 0.0);
	// glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, 1.0);
	glPushMatrix();
	glTranslatef(0.01, 1, 0);
	glRasterPos2f(0, -.05+offset);
	other_font->Render(this->correction ? "  aspect: corrected" : "  aspect: stretched");
	sprintf( buffer, " (%f)", this->aspect);
	other_font->Render(buffer);



	glRasterPos2f(0, -.09+offset);
	other_font->FaceSize((unsigned)(18*(this->vh/512.0)));

	sprintf( buffer, " texsize: %d", this->renderTarget->texsize);
	other_font->Render(buffer);

	glRasterPos2f(0, -.13+offset);
	sprintf( buffer, "viewport: %d x %d", this->vw, this->vh);

	other_font->Render(buffer);
	glRasterPos2f(0, -.17+offset);
	other_font->Render((this->renderTarget->useFBO ? "     FBO: on" : "     FBO: off"));

	glRasterPos2f(0, -.21+offset);
	sprintf( buffer, "    mesh: %d x %d", mesh.width, mesh.height);
	other_font->Render(buffer);

	glRasterPos2f(0, -.25+offset);
	sprintf( buffer, "textures: %.1fkB", textureManager->getTextureMemorySize() /1000.0f);
	other_font->Render(buffer);

	glPopMatrix();
	// glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);



#endif /** USE_FTGL */
}
void Renderer::draw_fps( float realfps )
{
#ifdef USE_FTGL
	char bufferfps[20];
	sprintf( bufferfps, "%.1f fps", realfps);
	// glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);

	glColor4f(1.0, 1.0, 1.0, 1.0);
	glPushMatrix();
	glTranslatef(0.01, 1, 0);
	glRasterPos2f(0, -0.05);
	title_font->FaceSize((unsigned)(20*(this->vh/512.0)));
	title_font->Render(bufferfps);

	glPopMatrix();
	// glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

#endif /** USE_FTGL */
}



void Renderer::CompositeOutput(const Pipeline* pipeline)
{

	int flipx=1, flipy=1;

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//Overwrite anything on the screen
	glBlendFunc(GL_ONE, GL_ZERO);
	glColor4f(1.0, 1.0, 1.0, 1.0f);

	glEnable(GL_TEXTURE_2D);

#ifdef USE_CG
  cgGLBindProgram(myCgCompositeProgram);
  checkForCgError("binding warp program");

  cgGLEnableProfile(myCgProfile);
  checkForCgError("enabling warp profile");
#endif


	float tex[4][2] = {{0, 1},
			   {0, 0},
			   {1, 0},
			   {1, 1}};

	float points[4][2] = {{-0.5, -0.5},
			      {-0.5,  0.5},
			      { 0.5,  0.5},
			      { 0.5,  -0.5}};

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2,GL_FLOAT,0,points);
	glTexCoordPointer(2,GL_FLOAT,0,tex);

	glDrawArrays(GL_TRIANGLE_FAN,0,4);

	//Noe Blend the Video Echo
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_TEXTURE);

	//draw video echo
	glColor4f(1.0, 1.0, 1.0, pipeline->videoEchoAlpha);
	glTranslatef(.5, .5, 0);
	glScalef(1.0/pipeline->videoEchoZoom, 1.0/pipeline->videoEchoZoom, 1);
	glTranslatef(-.5, -.5, 0);

	switch ((int)pipeline->videoEchoOrientation)
	{
		case 0: flipx=1;flipy=1;break;
		case 1: flipx=-1;flipy=1;break;
		case 2: flipx=1;flipy=-1;break;
		case 3: flipx=-1;flipy=-1;break;
		default: flipx=1;flipy=1; break;
	}

	float pointsFlip[4][2] = {{-0.5*flipx, -0.5*flipy},
				  {-0.5*flipx,  0.5*flipy},
				  { 0.5*flipx,  0.5*flipy},
				  { 0.5*flipx, -0.5*flipy}};

	glVertexPointer(2,GL_FLOAT,0,pointsFlip);
	glDrawArrays(GL_TRIANGLE_FAN,0,4);

	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef USE_CG
  cgGLUnbindProgram(myCgProfile);
  checkForCgError("disabling fragment profile");
  cgGLDisableProfile(myCgProfile);
  checkForCgError("disabling fragment profile");
#endif

	for (std::vector<RenderItem*>::const_iterator pos = pipeline->compositeDrawables.begin(); pos != pipeline->compositeDrawables.end(); ++pos)
			(*pos)->Draw(renderContext);





}


//Actually draws the texture to the screen
//
//The Video Echo effect is also applied here
void Renderer::render_texture_to_screen(PresetOutputs *presetOutputs)
{

	int flipx=1, flipy=1;

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//Overwrite anything on the screen
	glBlendFunc(GL_ONE, GL_ZERO);
	glColor4f(1.0, 1.0, 1.0, 1.0f);

	glEnable(GL_TEXTURE_2D);



	float tex[4][2] = {{0, 1},
			   {0, 0},
			   {1, 0},
			   {1, 1}};

	float points[4][2] = {{-0.5, -0.5},
			      {-0.5,  0.5},
			      { 0.5,  0.5},
			      { 0.5,  -0.5}};

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2,GL_FLOAT,0,points);
	glTexCoordPointer(2,GL_FLOAT,0,tex);

	glDrawArrays(GL_TRIANGLE_FAN,0,4);

	//Noe Blend the Video Echo
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_TEXTURE);

	//draw video echo
	glColor4f(1.0, 1.0, 1.0, presetOutputs->fVideoEchoAlpha);
	glTranslatef(.5, .5, 0);
	glScalef(1.0/presetOutputs->fVideoEchoZoom, 1.0/presetOutputs->fVideoEchoZoom, 1);
	glTranslatef(-.5, -.5, 0);

	switch (((int)presetOutputs->nVideoEchoOrientation))
	{
		case 0: flipx=1;flipy=1;break;
		case 1: flipx=-1;flipy=1;break;
		case 2: flipx=1;flipy=-1;break;
		case 3: flipx=-1;flipy=-1;break;
		default: flipx=1;flipy=1; break;
	}

	float pointsFlip[4][2] = {{-0.5*flipx, -0.5*flipy},
				  {-0.5*flipx,  0.5*flipy},
				  { 0.5*flipx,  0.5*flipy},
				  { 0.5*flipx, -0.5*flipy}};

	glVertexPointer(2,GL_FLOAT,0,pointsFlip);
	glDrawArrays(GL_TRIANGLE_FAN,0,4);

	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (presetOutputs->bBrighten==1)
	{
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_ZERO, GL_DST_COLOR);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	}

	if (presetOutputs->bDarken==1)
	{
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_ZERO, GL_DST_COLOR);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}


	if (presetOutputs->bSolarize)
	{
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_DST_COLOR);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_DST_COLOR, GL_ONE);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (presetOutputs->bInvert)
	{
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
		glDrawArrays(GL_TRIANGLE_FAN,0,4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

}
