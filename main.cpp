// STD
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <algorithm>

// GLEW
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glext.h>

// SOIL
#include <GL/SOIL/SOIL.h>

// OpenGL
#include <GL/GL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// SDL
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>

// Utils
#include <Utils\ShaderUtils.h>

// OVR
#define OVR_OS_WIN32
#include <OVR.h>
#include <OVRVersion.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>


// Main Header
#include "main.h"

// Model (Vertices)
#include "Model.h"




int main(int argc, char *argv[])
{

	if (argc > 1){
		if  (strcmp(argv[1], "-debug") == 0 )
			mode = MODE_DEBUG;
		else if ( strcmp(argv[1], "-oculus") == 0 )
			mode = MODE_OCULUS;
		else if ( strcmp(argv[1], "-oculus-debug") == 0 )
			mode = MODE_OCULUS_DEBUG;
		else return 100;
	}else
		mode = MODE_DEBUG;

	int err;

	// Init OVR library, hardware and sensors.
	err = init_ovr();
	if ( err != 0 )
		exit( 10 + err );

	//Init windows and OpenGL context
	err = init_SDL_GL();
	if ( err != 0 )
		exit( 0 + err );
	
	// Load and Init shader and shaders Program
	err = load_init_shaders();
	if ( err != 0 )
		exit( 20 + err );

	// Load the Vertices, vertex arrays, etc... And bind them, along with the shaders.
	err = load_vertex();
	if ( err != 0 )
		exit( 30 + err );

	// Loads the texture from files and bien them as uniform in the frag shader
	err = load_textures();
	if ( err != 0 )
		exit( 40 + err );

	if (mode != MODE_DEBUG){
		// Inits the frame buffer, usefull for rendering the scene in a texture to send it to Oculus
		err = init_framebuffers();
		if ( err != 0 )
			exit( 50 + err );

		err = init_render_ovr();
		if ( err != 0 )
			exit( 60 + err );
	}

	std::cout << "Recommended w " << recommendedTex0Size.w << std::endl << "Recommended h " << recommendedTex0Size.h << std::endl;

	// Tansformations
	//---------------------------------------------
	// ---- Transfo
	glm::mat4 trans;
	GLuint uniTrans = glGetUniformLocation(shaderProgram, "trans");

	// ---- View
	glm::mat4 view;
	view = glm::lookAt(
		glm::vec3(1.2f, 1.2f, 1.2f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	GLint uniView = glGetUniformLocation(shaderProgram, "view");
	glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

	// ---- Projection
	glm::mat4 proj;
	if ( mode == MODE_DEBUG )
		proj = glm::perspective(45.0f, 1280.0f / 720.0f, 1.0f, 10.0f);
	else
		proj = glm::perspective(45.0f, 640.0f / 720.0f, 1.0f, 10.0f);

	GLint uniProj = glGetUniformLocation(shaderProgram, "proj");
	glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));
	
	


	// Render in Texture, and display
	//-------------------------------------------------
	if (mode == MODE_OCULUS_DEBUG ){

		load_init_passthrough_shaders();
		GLuint passthroughOB;
		glGenBuffers(1, &passthroughOB);
		glBindBuffer(GL_ARRAY_BUFFER, passthroughOB);
		glBufferData(GL_ARRAY_BUFFER, sizeof(passthroughScreen), passthroughScreen, GL_STATIC_DRAW);

		// Binding the fragment Shader output to the current buffer
		glBindFragDataLocation(passthroughShadersProgram, 0, "passthroughColor");

		// Link and Use Program
		glLinkProgram(passthroughShadersProgram);
		glUseProgram(passthroughShadersProgram);

		// Store the attributes for the shaders
		
		glGenVertexArrays(1, &passthroughVAO);
		glBindVertexArray(passthroughVAO);

		// Attributes Locations for Shaders and Enable
		GLint posAttrib = glGetAttribLocation(passthroughShadersProgram, "position");
		glVertexAttribPointer(posAttrib, 0, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*) 2 );
		glEnableVertexAttribArray(posAttrib);

		GLint colorAttrib = glGetAttribLocation(passthroughShadersProgram, "texCoords");
		glVertexAttribPointer(colorAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2) );
		glEnableVertexAttribArray(colorAttrib);

		glUseProgram(passthroughShadersProgram);
		glUniform1i(glGetUniformLocation(passthroughShadersProgram, "renderedTex"), 0);
	}



	// Event Loop
	//--------------------------------------------------
	SDL_Event windowEvent;
	while (true)
	{
		if (SDL_PollEvent(&windowEvent))
		{

			// Quit events
			if (windowEvent.type == SDL_QUIT) break;
			else if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE) break;

		}
		



		if ( mode == MODE_OCULUS || mode == MODE_OCULUS_DEBUG ){

			ovrFrameTiming hdmFrameTiming = ovrHmd_BeginFrame(hmd, 0);

			glBindVertexArray(vertexArrayObject);
			glUseProgram(shaderProgram);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[0]);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, textures[1]);

			for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++){

				ovrEyeType eye = hmdDesc.EyeRenderOrder[eyeIndex];
				ovrPosef eyePose = ovrHmd_BeginEyeRender(hmd, eye);

				// Clear the screen and the depth buffer (as it is filled with 0 initially, 
				// nothing will be draw (0 = on top);
				glClearColor(0.0f, 0.0f, 0.3f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_FRAMEBUFFER);

				// Drawing in the FrameBuffer
				glBindBuffer(GL_FRAMEBUFFER, frameBuffer);
				glBindRenderbuffer(GL_RENDERBUFFER, rboDepthStencil);
				glEnable(GL_DEPTH_TEST);
				
				
				
				if (eye == ovrEye_Right){
					glScissor(renderTargetSize.w / 2, 0, renderTargetSize.w / 2, renderTargetSize.h);
					glViewport(renderTargetSize.w / 2, 0, renderTargetSize.w / 2, renderTargetSize.h);
				}else{
					glScissor(0, 0, renderTargetSize.w / 2, renderTargetSize.h);
					glViewport(0, 0, renderTargetSize.w / 2, renderTargetSize.h);
				}

				if (eye == ovrEye_Right)
					glClearColor(0.0f, 0.3f, 0.0f, 1.0f);
				else
					glClearColor(0.3f, 0.0f, 0.0f, 1.0f);

				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



				//Turn around Z
				trans = glm::rotate(
					trans,
					0.7f,
					glm::vec3(0.0f, 0.0f, 1.0f)
				);
				glUniformMatrix4fv(uniTrans, 1, GL_FALSE, glm::value_ptr(trans));
				
				// Drawing
				glDrawArrays(GL_TRIANGLES, 0, 36);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);
				glDisable(GL_DEPTH_TEST);


				ovrHmd_EndEyeRender(hmd, eye, eyePose, &EyeTexture[eye].Texture);
		
			}
			
		
			ovrHmd_EndFrame(hmd);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glUseProgram(0);

			if ( mode == MODE_OCULUS_DEBUG ){
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glBindVertexArray(passthroughVAO);
				glDisable(GL_DEPTH_TEST);
				glUseProgram(passthroughShadersProgram);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, renderedTex);

				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		
		}else if (mode == MODE_DEBUG){


			// Clear the screen and the depth buffer (as it is filled with 0 initially, 
			// nothing will be draw (0 = on top);
			glClearColor(0.0f, 0.3f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//Turn around Z
			trans = glm::rotate(
				trans,
				1.0f,
				glm::vec3(0.0f, 0.0f, 1.0f)
			);
			glUniformMatrix4fv(uniTrans, 1, GL_FALSE, glm::value_ptr(trans));

			// Drawing
			glDrawArrays(GL_TRIANGLES, 0, 36);

		}


		if ( mode != MODE_OCULUS )
			SDL_GL_SwapWindow(window);


	}





	// Destoy the HMD and shutdown the library
	ovrHmd_Destroy(hmd);
	ovr_Shutdown();

	// Quite SDL and OpenGL
	glDeleteFramebuffers(1, &frameBuffer);
	SDL_GL_DeleteContext(context);
	SDL_Quit();

	return 0;
}



/**
 * ERRORCODE 0 => OK
 * ERRORCODE 1 => No HMD found
 * ERRORCODE 2 => Sensor Start failed
 */
int init_ovr(){

	// Init the OVR library
	ovr_Initialize();

	// Create the software device and connect the physical device
	hmd = ovrHmd_Create(0);
	if (!hmd) hmd = ovrHmd_CreateDebug(ovrHmd_DK1);
	if (hmd)
		ovrHmd_GetDesc( hmd, &hmdDesc );
	else
		return 1;

	// Starts the sensor input with check required Capabilities
	 if ( !ovrHmd_StartSensor(hmd, hmdDesc.SensorCaps, ovrSensorCap_Orientation) )
		 return 2;

	 if ( mode == MODE_DEBUG ){
		 recommendedTex0Size = OVR::Sizei ( 1280, 720 );
		 recommendedTex1Size = OVR::Sizei ( 1280, 720 );
		 renderTargetSize.w = 1280;
		 renderTargetSize.h = 720;
	 }else{
		//Configuring the Texture size (bigger than screen for barreldistortion)
		recommendedTex0Size = ovrHmd_GetFovTextureSize(hmd, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1.0f);
		recommendedTex1Size = ovrHmd_GetFovTextureSize(hmd, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1.0f);

		renderTargetSize.w = recommendedTex0Size.w + recommendedTex1Size.w;
		renderTargetSize.h = std::max( recommendedTex0Size.h, recommendedTex1Size.h );
	 }
	
	 return 0;
}



/**
 * ERRORCODE 0 => Success
 * ERRORCODE 1 => SDL_Init error
 * ERRORCODE 2 => Create Window Error
 * ERRORCODE 3 => GLEW init error
 * ERRORCODE 4 => Unable to query HWND from SDL2
 */
int init_SDL_GL(){

	// SDL Init
	if ( SDL_Init(SDL_INIT_EVERYTHING) != 0 )
		return 1;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	// Setup to be able to get the HWND out of SDL2 for OVR Rendering
	SDL_VERSION(&sdl_window_info.version);
	sdl_window_info.subsystem = SDL_SYSWM_WINDOWS;
	

	// GL Context
	if (mode == MODE_DEBUG)
		window = SDL_CreateWindow("OpenGL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL);
	else if (mode == MODE_OCULUS)
		window = SDL_CreateWindow("OpenGL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, hmdDesc.Resolution.w, hmdDesc.Resolution.h, SDL_WINDOW_OPENGL);
	else if (mode == MODE_OCULUS_DEBUG)
		window = SDL_CreateWindow("OpenGL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderTargetSize.w, renderTargetSize.h, SDL_WINDOW_OPENGL);

	if (!window)
		return 2;
	context = SDL_GL_CreateContext(window);

	if (!SDL_GetWindowWMInfo( window, &sdl_window_info ))
		return 4;

	// GLEW Init
	glewExperimental = GL_TRUE;
	if ( glewInit() != GLEW_OK )
		return 3;

	std::cout << "OpenGL version " << glGetString(GL_VERSION) << std::endl;

	// Enable the depth Buffer to prevent overlaping
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);

	return 0;
}

/**
 * ERRORCODE 0 => OK
 * ERRORCODE 1 => Shader loading error
 * ERRORCODE 2 => Shader compilation error
 */
int load_init_passthrough_shaders(){


	// ---- Vertex
	char * vertexShaderSourceFile = load_shader("Shaders/passthrough.vxs.glsl");
	if ( vertexShaderSourceFile == "FILE_ER" || vertexShaderSourceFile == "MEM_ER" || vertexShaderSourceFile == "READ_ER" )
		return 1;
	// Conversion char * -> GLchar *
	const GLchar * vertexShaderSource = vertexShaderSourceFile;

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	// Get Compilation status
	GLint vStatus;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vStatus);
	if (vStatus == GL_TRUE)
		std::cout << "Vertex shader load OK" << std::endl;
	else
		return 2;
	char vBuffer[512];
	glGetShaderInfoLog(vertexShader, 512, NULL, vBuffer);
	std::cout << "Log:" << std::endl << vBuffer << std::endl;


	// ---- Fragment
	char * fragmentShaderSourceFile = load_shader("Shaders/passthrough.fgs.glsl");
	if ( fragmentShaderSourceFile == "FILE_ER" || fragmentShaderSourceFile == "MEM_ER" || fragmentShaderSourceFile == "READ_ER" )
		return 1;

	const GLchar * fragmentShaderSource = fragmentShaderSourceFile;

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	GLint fStatus;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fStatus);
	if (fStatus == GL_TRUE)
		std::cout << "Fragment shader load OK" << std::endl;
	else
		return 2;
	char fBuffer[512];
	glGetShaderInfoLog(fragmentShader, 512, NULL, fBuffer);
	std::cout << "Log:" << std::endl << fBuffer << std::endl;

	// ---- Create Program for shaders
	passthroughShadersProgram = glCreateProgram();
	glAttachShader(passthroughShadersProgram, vertexShader);
	glAttachShader(passthroughShadersProgram, fragmentShader);

	return 0;
}

/**
 * ERRORCODE 0 => OK
 * ERRORCODE 1 => Shader loading error
 * ERRORCODE 2 => Shader compilation error
 */
int load_init_shaders(){


	// ---- Vertex
	char * vertexShaderSourceFile = load_shader("Shaders/basic.vxs.glsl");
	if ( vertexShaderSourceFile == "FILE_ER" || vertexShaderSourceFile == "MEM_ER" || vertexShaderSourceFile == "READ_ER" )
		return 1;
	// Conversion char * -> GLchar *
	const GLchar * vertexShaderSource = vertexShaderSourceFile;

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	// Get Compilation status
	GLint vStatus;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vStatus);
	if (vStatus == GL_TRUE)
		std::cout << "Vertex shader load OK" << std::endl;
	else
		return 2;
	char vBuffer[512];
	glGetShaderInfoLog(vertexShader, 512, NULL, vBuffer);
	std::cout << "Log:" << std::endl << vBuffer << std::endl;


	// ---- Fragment
	char * fragmentShaderSourceFile = load_shader("Shaders/white.fgs.glsl");
	if ( fragmentShaderSourceFile == "FILE_ER" || fragmentShaderSourceFile == "MEM_ER" || fragmentShaderSourceFile == "READ_ER" )
		return 1;

	const GLchar * fragmentShaderSource = fragmentShaderSourceFile;

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	GLint fStatus;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fStatus);
	if (fStatus == GL_TRUE)
		std::cout << "Fragment shader load OK" << std::endl;
	else
		return 2;
	char fBuffer[512];
	glGetShaderInfoLog(fragmentShader, 512, NULL, fBuffer);
	std::cout << "Log:" << std::endl << fBuffer << std::endl;

	// ---- Create Program for shaders
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);

	return 0;
}

int load_vertex(){

	// Vertexbuffer and Attributes for shaders
	//------------------------------------------------------
	// Création du vertex buffer
	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

	// Binding the fragment Shader output to the current buffer
	glBindFragDataLocation(shaderProgram, 0, "outColor");

	// Link and Use Program
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);

	// Store the attributes for the shaders
	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

	// Attributes Locations for Shaders and Enable
	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	glVertexAttribPointer(posAttrib, indexColors - indexCoords, GL_FLOAT, GL_FALSE, sizeof(float) * vColumns, (void*) indexCoords );
	glEnableVertexAttribArray(posAttrib);

	GLint colorAttrib = glGetAttribLocation(shaderProgram, "color");
	glVertexAttribPointer(colorAttrib, indexUV - indexColors, GL_FLOAT, GL_FALSE, sizeof(float) * vColumns, (void*)(sizeof(float) * indexColors) );
	glEnableVertexAttribArray(colorAttrib);

	GLint texAttrib = glGetAttribLocation(shaderProgram, "texCoords");
	glVertexAttribPointer(texAttrib, vColumns - indexUV, GL_FLOAT, GL_FALSE, sizeof(float) * vColumns, (void*)(sizeof(float) * indexUV) );
	glEnableVertexAttribArray(texAttrib);

	/*
	// Element Buffer Array
	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
	*/
	
	return 0;

}

/**
 * ERRORCODE 0 => OK
 * ERRORCODE 1 => Failed to load the texture
 */
int load_textures(){
	// Texture
	//-----------------------------------------------
	// Creating Texture
	glGenTextures(2, textures);

	// ---- Grid
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	// Loading the image Grid
	int width, height;
	unsigned char * image = SOIL_load_image("img_grid.bmp", &width, &height, 0, SOIL_LOAD_RGB);
	if ( image == NULL )
		return 1;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	SOIL_free_image_data(image);
	glUniform1i(glGetUniformLocation(shaderProgram, "texGrid"), 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	// ---- Star
	glActiveTexture(GL_TEXTURE1);  
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	// Loading the image Star
	image = SOIL_load_image("img_star.bmp", &width, &height, 0, SOIL_LOAD_RGB);
	if ( image == NULL )
		return 1;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	SOIL_free_image_data(image);
	glUniform1i(glGetUniformLocation(shaderProgram, "texStar"), 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	return 0;

}

int init_framebuffers(){

	// Framebuffers
	//-----------------------------------------------
	// In order to display, it has to be "complete" (at least 1 color/depth/stencil buffer attached, 1color attachement, same number of multisamples, attachement completes)
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);


	// ---- Texture "Color Buffer"
	glGenTextures(1, &renderedTex);
	glBindTexture(GL_TEXTURE_2D, renderedTex);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, renderTargetSize.w , renderTargetSize.h, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	// Attaching the color buffer to the frame Buffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTex, 0);



	// ---- RenderBuffer
	// Render Buffer (to be able to render Depth calculation)
	glGenRenderbuffers(1, &rboDepthStencil);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepthStencil);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, renderTargetSize.w , renderTargetSize.h);
	
	// Attaching the render buffer to the framebuffer
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepthStencil);
	
	GLenum l_GLDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, l_GLDrawBuffers); // "1" is the size of DrawBuffers
	
	GLenum l_Check = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
       if (l_Check!=GL_FRAMEBUFFER_COMPLETE)
       {
          printf("There is a problem with the FBO.\n");
          //exit(199);
       }

	return 0;
}

/**
 * ERRORODE 0 => OK
 * ERRORCODE 1 => Unable to configure OVR Render
 */
int init_render_ovr(){

	// Configure and Init rendering using the OVR render Core.
	// Input => rendered 3D texture (Two passes: 1 Left, 1 Right)
	// Output auto, on defined window

	// Configure rendering with OpenGL
	ovrGLConfig cfg;
	cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
	cfg.OGL.Header.RTSize = OVR::Sizei( hmdDesc.Resolution.w, hmdDesc.Resolution.h );
	cfg.OGL.Header.Multisample = 0;
	cfg.OGL.Window = sdl_window_info.info.win.window;

	ovrFovPort eyesFov[2] =  { hmdDesc.DefaultEyeFov[0], hmdDesc.DefaultEyeFov[1] };
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if ( mode == MODE_OCULUS ){
		if ( !ovrHmd_ConfigureRendering(hmd, &cfg.Config, 0, eyesFov, eyesRenderDesc) )
			return 1;
	}

	std::cout << "distorted viewport w: " << eyesRenderDesc[0].DistortedViewport.Size.w << std::endl;

	EyeTexture[0].OGL.Header.API = ovrRenderAPI_OpenGL;
	EyeTexture[0].OGL.Header.TextureSize = renderTargetSize;
	EyeTexture[0].OGL.Header.RenderViewport.Size = recommendedTex0Size;
	EyeTexture[0].OGL.Header.RenderViewport.Pos.x = 0;
	EyeTexture[0].OGL.Header.RenderViewport.Pos.y = 0;
	EyeTexture[0].OGL.TexId = renderedTex;

	EyeTexture[1] = EyeTexture[0];
	EyeTexture[1].OGL.Header.RenderViewport.Pos.x = recommendedTex0Size.w / 2;

	return 0;
}
