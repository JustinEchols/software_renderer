
//
// NOTE(Justin) Transformations. As of right now Barycentric
// coordinates are not implemented. Each of the transformations
// below is applied to the position vectors of the three
// vertices of the triangle. This works for translations of a
// triangle but not for rotations or scaling transformations. 
// Scaling the position vectors of the three vertices produces a
// shift. Rotating the position vectors of the three vertices
// will produce a rotation of the trignle. However each vertex
// will rotate by a different amount based off the length of the
// position vector. 
//

#include <windows.h>

#include "software_renderer_types.h"
#include "software_renderer_math.h"

global_variable b32 GLOBAL_RUNNING;
global_variable win32_back_buffer Win32BackBuffer;
global_variable LARGE_INTEGER tick_frequency;
global_variable input Input;


internal void
scan_buffer_fill_between_vertices(scan_buffer *ScanBuffer, v3f VertexMin, v3f VertexMax, b32 which_side)
{
	// 
	// NOTE(Justin): Implementing TOP-LEFT fill convention using the ceiling
	// function.  Is there any easy to implement this? Round and add 1?
	//

	s32 y_start	= (s32)ceil(VertexMin.y);
	s32 y_end	= (s32)ceil(VertexMax.y);
	s32 x_start	= (s32)ceil(VertexMin.x);
	s32 x_end	= (s32)ceil(VertexMax.x);

	f32 y_dist = VertexMax.y - VertexMin.y;
	f32 x_dist = VertexMax.x - VertexMin.x;

	f32 x_step = (f32)x_dist / (f32)y_dist;

	// Distance between real y and first scanline
	f32 yprestep = y_start - VertexMin.y;
	//f32 x_current = (f32)x_start;

	f32 x_current = VertexMin.x + yprestep * x_step;

	s32 *scan_buffer_memory = (s32 *)ScanBuffer->memory;
	for (s32 j = y_start; j < y_end; j++) {
		scan_buffer_memory[2 * j + which_side] = (s32)ceil(x_current);
		x_current += x_step;
	}
}

internal void
scan_buffer_fill_triangle(scan_buffer *ScanBuffer, triangle *Triangle)
{
	v3f tmp = {};
	v3f *min = &Triangle->Vertices[0].xyz;
	v3f *mid = &Triangle->Vertices[1].xyz;
	v3f *max = &Triangle->Vertices[2].xyz;

	if (min->y > max->y) {
		tmp = *min;
		*min = *max;
		*max = tmp;
	}
	if (mid->y >  max->y) {
		tmp = *mid;
		*mid = *max;
		*max = tmp;
	}
	if (min->y > mid->y) {
		tmp = *min;
		*min = *mid;
		*mid = tmp;
	}

	//
	// NOTE(Justin): To determine whether or not the triangle is right oriented 
	// or left oriented, find the signed area of the triangle using the cross product
	// of two vectors which represent two sides of the triangle. The first vector
	// is the vector FROM MAX TO MIN. The second is FROM MAX TO MID. Note that
	// we are using the sign to determine the orientation of the triangle.
	//

	f32 v0_x = min->x - max->x;
	f32 v0_y = min->y - max->y;

	f32 v1_x = mid->x - max->x;
	f32 v1_y = mid->y - max->y;

	f32 area_double_signed = v0_x * v1_y - v1_x * v0_y;

	b32 oriented_right;
	if (area_double_signed > 0) {
		oriented_right = true;
	} else {
		oriented_right = false;
	}

	//
	// NOTE(Justin): This routine fills the minimmum x values first in the scan
	// buffer. This corresponds to all x values of the side of the triangle that
	// is right oriented and has only one edge. The indexing # to fill the scan buffer is 
	// (2 * j + wihch_side). We want the minimum values to be at offsets 0, 2, 4, ...
	// and so on in the scann buffer. That is why we set which_side to false
	// because then (2 * j + which_side(0))  is even for each j.
	// , initially, whenever the triangle is right oriented. After the min
	// values are entered in the scan buffer the max values for the remaining
	// two sides of the triangle are entered into the scan buffer at offsets 1,B
	// 3, 5, 7, ... which are odd offsets. This is why we set which_side = true
	// after. Then 2 * j + which_side is odd for each value of j. 
	//

	b32 which_side;
	if (oriented_right) {
		which_side = false;
	} else {
		which_side = true;
	}

	scan_buffer_fill_between_vertices(ScanBuffer, *min, *max, which_side);

	which_side = !which_side;
	scan_buffer_fill_between_vertices(ScanBuffer, *mid, *max, which_side);
	scan_buffer_fill_between_vertices(ScanBuffer, *min, *mid, which_side);
}

internal u32
color_convert_v3f_to_u32(v3f Color)
{
	u32 result = 0;

	u32 red		= (u32)(255.0f * Color.r);
	u32 green	= (u32)(255.0f * Color.g);
	u32 blue	= (u32)(255.0f * Color.b);
	result		= ((red << 16) | (green << 8) | (blue << 0));

	return(result);
}

internal void
scan_buffer_draw_shape(win32_back_buffer *Win32BackBuffer, scan_buffer *ScanBuffer, f32 ymin, f32 ymax, v3f Color)
{
	// TODO(Justin): ASSERT 

	int y_min = round_f32_to_s32(ymin);
	int y_max = round_f32_to_s32(ymax);

	if (y_min < 0) {
		y_min = 0;
	}
	if (y_max >= Win32BackBuffer->height) {
		y_max = Win32BackBuffer->height - 1;
	}

	u32 color = color_convert_v3f_to_u32(Color);

	u8 *pixel_row = (u8 *)Win32BackBuffer->memory + Win32BackBuffer->stride * y_min;
	s32 *scan_buffer_memory = (s32 *)ScanBuffer->memory;
	for (int y = y_min; y < y_max; y++) {
		int x_min = scan_buffer_memory[2 * y];
		int x_max = scan_buffer_memory[2 * y + 1];

		u8 *pixel_start = pixel_row + Win32BackBuffer->bytes_per_pixel * x_min;

		u32 *pixel = (u32 *)pixel_start;
		for (int i = x_min; i < x_max; i++) {
			*pixel++ = color;
		}
		pixel_row += Win32BackBuffer->stride;
	}
}



internal void
pixel_set(win32_back_buffer *Win32BackBuffer, v2f PixelPos, v3f Color)
{
	int x = (int)(PixelPos.x + 0.5f);
	int y = (int)(PixelPos.y + 0.5f);

	if ((x < 0) || (x >= Win32BackBuffer->width)) {
		return;
	}
	if ((y < 0) || (y >= Win32BackBuffer->height)) {
		return;
	}

	u32 color = color_convert_v3f_to_u32(Color);

	u8 *pixel_row = (u8 *)Win32BackBuffer->memory + Win32BackBuffer->stride * y + Win32BackBuffer->bytes_per_pixel * x;
	u32 *pixel = (u32 *)pixel_row;
	*pixel = color;
}

internal v3f
color_rand_init()
{
	v3f Result;
	Result.r = ((f32)rand() / (f32)RAND_MAX);
	Result.g = ((f32)rand() / (f32)RAND_MAX);
	Result.b = ((f32)rand() / (f32)RAND_MAX);

	return(Result);
}

internal v4f
v4f_rand_init()
{
	v4f Result = {};

	Result.x = ((((f32)rand() / (f32)RAND_MAX) * 2.0f) - 1.0f);
	Result.y = ((((f32)rand() / (f32)RAND_MAX) * 2.0f) - 1.0f);
	Result.z = (((f32)rand() / (f32)RAND_MAX) - 2.0f);
	Result.w = 1;

	return(Result);
}

internal triangle
triangle_rand_init()
{
	triangle Result = {};
	for(u32 i = 0; i < 3; i++) {
		Result.Vertices[i] = v4f_rand_init();
	}
	Result.Color = color_rand_init();
	return(Result);
}

internal void
line_draw_dda(win32_back_buffer *Win32BackBuffer, v2f P1, v2f P2, v3f Color)
{
	v2f Diff = P2 - P1;

	int dx = round_f32_to_s32(Diff.x);
	int dy = round_f32_to_s32(Diff.y);

	int steps, k;

	if (ABS(dx) > ABS(dy)) {
		steps = ABS(dx);
	} else {
		steps = ABS(dy);
	}

	v2f Increment = {(f32)dx / (f32)steps, (f32)dy / (f32)steps};
	v2f PixelPos = P1;


	pixel_set(Win32BackBuffer, PixelPos, Color);
	for (k = 0; k < steps; k++) {
		PixelPos += Increment;
		pixel_set(Win32BackBuffer, PixelPos, Color);
	}
}

#if 0
	internal void
cube_draw(win32_back_buffer *Win32BackBuffer, cube Cube, v3f Color)
{
	line_draw_dda(Win32BackBuffer, P0)
}
#endif


internal void
win32_back_buffer_resize(win32_back_buffer *Win32BackBuffer, int width, int height)
{
	if (Win32BackBuffer->memory) {
		VirtualFree(Win32BackBuffer->memory, 0, MEM_RELEASE);
	}

	Win32BackBuffer->width = width;
	Win32BackBuffer->height = height;
	Win32BackBuffer->bytes_per_pixel = 4;
	Win32BackBuffer->stride = Win32BackBuffer->width * Win32BackBuffer->bytes_per_pixel;

	Win32BackBuffer->Info.bmiHeader.biSize = sizeof(Win32BackBuffer->Info.bmiHeader);
	Win32BackBuffer->Info.bmiHeader.biWidth = Win32BackBuffer->width;
	Win32BackBuffer->Info.bmiHeader.biHeight = Win32BackBuffer->height;
	Win32BackBuffer->Info.bmiHeader.biPlanes = 1;
	Win32BackBuffer->Info.bmiHeader.biBitCount = 32;
	Win32BackBuffer->Info.bmiHeader.biCompression = BI_RGB;


	int Win32BackBuffersize = Win32BackBuffer->width * Win32BackBuffer->height * Win32BackBuffer->bytes_per_pixel;
	Win32BackBuffer->memory = VirtualAlloc(0, Win32BackBuffersize, (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);
}

internal LRESULT CALLBACK
win32_main_window_callback(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch (Message) {
		case WM_CLOSE:
		case WM_DESTROY:
		{
			GLOBAL_RUNNING = false;
		} break;
		case WM_SIZE:
		{
			RECT Rect;
			GetClientRect(Window, &Rect);

			int client_width = Rect.right - Rect.left;
			int client_height = Rect.bottom - Rect.top;

			win32_back_buffer_resize(&Win32BackBuffer, client_width, client_height);

		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT PaintStruct;
			HDC DeviceContext = BeginPaint(Window, &PaintStruct);

			StretchDIBits(DeviceContext,
					0, 0, Win32BackBuffer.width, Win32BackBuffer.height,
					0, 0, Win32BackBuffer.width, Win32BackBuffer.height,
					Win32BackBuffer.memory,
					&Win32BackBuffer.Info,
					DIB_RGB_COLORS,
					SRCCOPY);

			EndPaint(Window, &PaintStruct);
		} break;
		default:
		{
			result = DefWindowProcA(Window, Message, wParam, lParam);
		}
	}
	return(result);
}

internal v4f
lerp_points(v4f P1, v4f P2, f32 t)
{
	v4f Result = (1.0f - t) * P1 + t * P2;
	return(Result);
}

internal v3f
lerp_color(v3f ColorA, v3f ColorB, f32 t)
{
	//TODO: ASSERT t in [0, 1]

	v3f Result = (1.0f - t) * ColorA + t * ColorB;
	return(Result);
}

internal void
circle_draw(win32_back_buffer *Win32BackBuffer, circle Circle, v3f Color)
{
	// Bounding Box
	u32 x_min = round_f32_to_u32(Circle.Center.x - Circle.radius);
	u32 x_max = round_f32_to_u32(Circle.Center.x + Circle.radius);

	u32 y_min = round_f32_to_u32(Circle.Center.y - Circle.radius);
	u32 y_max = round_f32_to_u32(Circle.Center.y + Circle.radius);

	f32 distance_squared = 0.0f;
	f32 radius_sqaured = SQUARE(Circle.radius);// * Circle.radius;

	f32 test_x, test_y;
	v2f test = {0.0f, 0.0f};
	for (u32 y = y_min; y < y_max; y++)  {
		for (u32 x = x_min; x < x_max; x++)  {
			test_x = Circle.Center.x - x;
			test_y = Circle.Center.y - y;

			distance_squared = SQUARE(test_x) + SQUARE(test_y);//test_x * test_x + test_y * test_y;
			if (distance_squared <= radius_sqaured) {
				test = {(f32)x, (f32)y};
				pixel_set(Win32BackBuffer, test, Color);
			}
		}
	}
}

internal void
rectangle_draw(win32_back_buffer *Win32BackBuffer, rectangle R, v3f Color)
{
	// TODO(Justin): Bounds checking
	u32 x_min = round_f32_to_u32(R.Min.x);
	u32 x_max = round_f32_to_u32(R.Max.x);

	u32 y_min = round_f32_to_u32(R.Min.y);
	u32 y_max = round_f32_to_u32(R.Max.y);


	u32 color = color_convert_v3f_to_u32(Color);

	u8 * pixel_row = (u8 *)Win32BackBuffer->memory + Win32BackBuffer->stride * y_min + Win32BackBuffer->bytes_per_pixel * x_min;
	for (u32 y = y_min; y < y_max; y++) {
		u32 *pixel = (u32 *)pixel_row;
		for (u32 x = x_min; x < x_max; x++) {
			*pixel++ = color;
		}
		pixel_row += Win32BackBuffer->stride;
	}
}


//TODO(Justin): This is not rendering properly. Moving in and out in the z
//direction warps the axes.

#if 1
internal void
axis_draw(win32_back_buffer *Win32BackBuffer, m4x4 M, v4f Position)
{
	v4f OffsetX = {0.5f, 0.0f, 0.0f, 0.0f};
	v4f OffsetY = {0.0f, 0.5f, 0.0f, 0.0f};
	v4f OffsetZ = {0.0f, 0.0f, -0.5f, 0.0f};

	v4f FragmentOrigin = M * Position;
	FragmentOrigin =  (1.0f / FragmentOrigin.w) * FragmentOrigin;

	v4f FragmentX = Position + OffsetX;
	FragmentX = M * FragmentX;
	FragmentX =  (1.0f / FragmentX.w) * FragmentX;

	v4f FragmentY = Position + OffsetY;
	FragmentY = M * FragmentY;
	FragmentY =  (1.0f / FragmentY.w) * FragmentY;

	v4f FragmentZ = Position + OffsetZ;
	FragmentZ = M * FragmentZ;
	FragmentZ =  (1.0f / FragmentZ.w) * FragmentZ;

	v3f Color = {1.0f, 0.0f, 0.0f};
	line_draw_dda(Win32BackBuffer, FragmentOrigin.xy, FragmentX.xy, Color);
	
	Color = {0.0f, 1.0f, 0.0f};
	line_draw_dda(Win32BackBuffer, FragmentOrigin.xy, FragmentY.xy, Color);

	Color = {0.0f, 0.0f, 1.0f};
	line_draw_dda(Win32BackBuffer, FragmentOrigin.xy, FragmentZ.xy, Color);
}
#endif


int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASSA WindowClass = {};

	WindowClass.style = (CS_HREDRAW | CS_VREDRAW);
	WindowClass.lpfnWndProc = win32_main_window_callback;
	WindowClass.hInstance = hInstance;
	WindowClass.lpszClassName = "Main window";

	Win32BackBuffer = {};
	win32_back_buffer_resize(&Win32BackBuffer, 960, 540);

	QueryPerformanceFrequency(&tick_frequency);
	f32 ticks_per_second = (f32)tick_frequency.QuadPart;
	f32 time_for_each_tick = 1.0f / ticks_per_second;

	if (RegisterClassA(&WindowClass)) {
		HWND Window = CreateWindowExA(
				0,
				WindowClass.lpszClassName,
				"Rasterization Test",
				(WS_OVERLAPPEDWINDOW | WS_VISIBLE),
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				0,
				0,
				hInstance,
				0);

		if (Window) {
			HDC DeviceContext = GetDC(Window);

			scan_buffer ScanBuffer;

			ScanBuffer.memory = VirtualAlloc(0, Win32BackBuffer.height * 2 * sizeof(u32), (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);
			ScanBuffer.height = Win32BackBuffer.height;


			f32 direction = 1.0f;
			v3f Color = {1.0f, 1.0f, 1.0f};

			v3f CameraPos = {0.0f, 0.0f, 0.0f};
			v3f CameraDirection = {0.0f, 0.0f, 1.0f};
			v3f CameraUp = {0.0f, 1.0f, 0.0f};


			v3f Camera2Pos = {1.0f, 1.0f, 1.0f};
			v3f Camera2Direction = {0.0f, 0.0f, 1.0f};
			v3f Camera2Up = {0.0f, 1.0f, 0.0f};

			v3f CameraAbovePos = {0.0f, 5.0f, -1.0f};
			v3f CameraAboveDirection = {0.0f, -1.0f, 0.0f};
			v3f CameraAboveUp = {0.0f, 0.0f, 1.0f};

			// TODO(Justin): changing the camera to produce a different viewing
			// perspective does not produce the expected result. For eaxmple,.
			// viewing the left down the negative x axis produces an inverted y
			// axis. fix debug this...
			//
			// NOTE(Justin): For a direction vector positive z values are
			// looking down the z axis and POSITIONS get more and more negative.
			// THIS IS CONFUSING. The direction you are looking has a positive z
			// value but the positions are negative :(

			m4x4 MapToCamera = m4x4_camera_map_create(CameraPos, CameraDirection, CameraUp);

			f32 l = -1.0f;
			f32 r = 1.0f;
			f32 b = -1.0f;
			f32 t = 1.0f;
			f32 n = 1.0f;
			f32 f = 2.0f;

			m4x4 MapToPersp = m4x4_perspective_projection_create(l, r, b, t, n, f);

			m4x4 MapToScreenSpace = m4x4_screen_space_map_create(Win32BackBuffer.width, Win32BackBuffer.height);
			m4x4 M = MapToScreenSpace * MapToPersp * MapToCamera;

			srand(2023);
#if 1
			triangle Triangles[3];

			for(u32 i = 0; i < ARRAY_COUNT(Triangles); i++) {
				Triangles[i] = triangle_rand_init();

			}

			triangle Fragment;
			Fragment = Triangles[0];
			for (u32 i = 0; i < 3; i++) {
				Fragment.Vertices[i] = M * Fragment.Vertices[i];
				Fragment.Vertices[i] = (1.0f / Fragment.Vertices[i].w ) * Fragment.Vertices[i];
			}
			scan_buffer_fill_triangle(&ScanBuffer, &Fragment);
#endif

			GLOBAL_RUNNING = true;

			b32 using_camera_one = true;
			b32 using_camera_two = false;
			b32 using_camera_three = false;


			f32 time_delta = 0.0f;
			f32 step = 0.0f;
			f32 step_sin = PI32 / 100.0f;

			LARGE_INTEGER tick_count_before;
			QueryPerformanceCounter(&tick_count_before);
			while (GLOBAL_RUNNING) {
				MSG Message;
				while (PeekMessage(&Message, Window, 0, 0, PM_REMOVE)) {
					switch (Message.message) {
						case WM_KEYDOWN:
						case WM_KEYUP: 
						{
							u32 vk_code = (u32)Message.wParam;
							b32 was_down = ((Message.lParam & (1 << 30)) != 0);
							b32 is_down = ((Message.lParam & (1 << 31)) == 0);

							if (was_down != is_down) {
								switch (vk_code) {
									case 'W':
									{
										Input.Buttons[BUTTON_UP].is_down = is_down;
									} break;
									case 'S':
									{
										Input.Buttons[BUTTON_DOWN].is_down = is_down;
									} break;
									case 'A':
									{
										Input.Buttons[BUTTON_LEFT].is_down = is_down;
									} break;
									case 'D':
									{
										Input.Buttons[BUTTON_RIGHT].is_down = is_down;
									} break;
									case VK_UP:
									{
										Input.Buttons[BUTTON_IN].is_down = is_down;
									} break;
									case VK_DOWN:
									{
										Input.Buttons[BUTTON_OUT].is_down = is_down;
									} break;
									case 0x31:
									{
										Input.Buttons[BUTTON_1].is_down = is_down;
									} break;
									case 0x32:
									{
										Input.Buttons[BUTTON_2].is_down = is_down;
									} break;
									case 0x33:
									{
										Input.Buttons[BUTTON_3].is_down = is_down;
									} break;
								}
							}
						} break;
						default:
						{
							TranslateMessage(&Message);
							DispatchMessage(&Message);
						}
					}
				}

				// Clear screen to black
				u32 *pixel = (u32 *)Win32BackBuffer.memory;
				for (s32 y = 0; y < Win32BackBuffer.height; y++) {
					for (s32 x = 0; x < Win32BackBuffer.width; x++) {
						*pixel++ = 0;
					}
				}

				// Resize the scan buffer upon a change in the window size
				if (ScanBuffer.height != Win32BackBuffer.height) {
					if (ScanBuffer.memory) {
						VirtualFree(ScanBuffer.memory, 0, MEM_RELEASE);
					}
					ScanBuffer.memory = VirtualAlloc(0, Win32BackBuffer.height * 2 * sizeof(u32), (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);
					ScanBuffer.height = Win32BackBuffer.height;

					MapToScreenSpace = m4x4_screen_space_map_create(Win32BackBuffer.width, Win32BackBuffer.height);
				}

				//
				// NOTE(Justin): Input/ Do something smarter here...
				//

				if (Input.Buttons[BUTTON_UP].is_down) {
					v3f Shift = {0.0f, 1.0f * time_delta, 0.0f};
					if (using_camera_one) {
						CameraPos += Shift;
					} else if (using_camera_two) {
						Camera2Pos += Shift;
					} else if (using_camera_three) {
						CameraAbovePos += Shift;
					}
				}
				if (Input.Buttons[BUTTON_DOWN].is_down) {
					v3f Shift = {0.0f, -1.0f * time_delta, 0.0f};
					if (using_camera_one) {
						CameraPos += Shift;
					} else if (using_camera_two) {
						Camera2Pos += Shift;
					} else if (using_camera_three) {
						CameraAbovePos += Shift;
					}
				}
				if (Input.Buttons[BUTTON_LEFT].is_down) {
					v3f Shift = {-1.0f * time_delta, 0.0f, 0.0f};
					if (using_camera_one) {
						CameraPos += Shift;
					} else if (using_camera_two) {
						Camera2Pos += Shift;
					} else if (using_camera_three) {
						CameraAbovePos += Shift;
					}
				}
				if (Input.Buttons[BUTTON_RIGHT].is_down) {
					v3f Shift = {1.0f * time_delta, 0.0f, 0.0f};
					if (using_camera_one) {
						CameraPos += Shift;
					} else if (using_camera_two) {
						Camera2Pos += Shift;
					} else if (using_camera_three) {
						CameraAbovePos += Shift;
					}
				}
				if (Input.Buttons[BUTTON_IN].is_down) {
					v3f Shift = {0.0f, 0.0f, -1.0f * time_delta};
					if (using_camera_one) {
						CameraPos += Shift;
					}
				}
				if (Input.Buttons[BUTTON_OUT].is_down) {
					v3f Shift = {0.0f, 0.0f, 1.0f * time_delta};
					if (using_camera_one) {
						CameraPos += Shift;
					} else if (using_camera_two) {
						Camera2Pos += Shift;
					} else if (using_camera_three) {
						CameraAbovePos += Shift;
					}
				}

				if (Input.Buttons[BUTTON_1].is_down) {
					using_camera_one = true;
					using_camera_two = false;
					using_camera_three = false;
				}
				if (Input.Buttons[BUTTON_2].is_down) {
					using_camera_one = false;
					using_camera_two = true;
					using_camera_three = false;
				}
				if (Input.Buttons[BUTTON_3].is_down) {
					using_camera_one = false;
					using_camera_two = false;
					using_camera_three = true;
				}

				//
				// NOTE(Justin): Update camera transform.
				//

				if (using_camera_one) {
					MapToCamera = m4x4_camera_map_create(CameraPos, CameraDirection, CameraUp);
				} else if (using_camera_two) {
					MapToCamera = m4x4_camera_map_create(Camera2Pos, Camera2Direction, Camera2Up);
				} else if (using_camera_three) {
					MapToCamera = m4x4_camera_map_create(CameraAbovePos, CameraAboveDirection, CameraAboveUp);
				}

				//
				// NOTE(Justin): Render axes
				//

				v4f tmp = {0.0f, 0.0f, -1.0f, 1};
#if 1

				tmp = MapToScreenSpace * MapToPersp * MapToCamera * tmp;
				tmp = (1.0f / tmp.w) * tmp;


				v4f OffsetZ = {0.0f, 0.0f, -1.0f, 0.0f};
				v4f tmp2 = {0.0f, 0.0f, -2.0f, 1};
				//v4f tmp2 = Position + OffsetZ;
				tmp2 = MapToScreenSpace * MapToPersp * MapToCamera * tmp2;
				tmp2 = (1.0f / tmp2.w) * tmp2;

				Color = {0.0f, 0.0f, 1.0f};
				line_draw_dda(&Win32BackBuffer, tmp.xy, tmp2.xy, Color);



				tmp2 = {1.0f, 0.0f, -1.0f, 1};
				tmp2 = MapToScreenSpace * MapToPersp * MapToCamera * tmp2;
				tmp2 = (1.0f / tmp2.w) * tmp2;

				Color = {1.0f, 0.0f, 0.0f};
				line_draw_dda(&Win32BackBuffer, tmp.xy, tmp2.xy, Color);

				tmp2 = {0.0f, 1.0f, -1.0f, 1};
				tmp2 = MapToScreenSpace * MapToPersp * MapToCamera * tmp2;
				tmp2 = (1.0f / tmp2.w) * tmp2;

				Color = {0.0f, 1.0f, 0.0f};
				line_draw_dda(&Win32BackBuffer, tmp.xy, tmp2.xy, Color);
#endif


#if 1
				
				//
				// NOTE(Justin): Render triangle
				//

				m4x4 RotateZ = m4x4_rotation_z_create(PI32 * time_delta);
				triangle Fragment;
				for (u32 i = 0; i < 3; i++) {
					Triangles[0].Vertices[i] = RotateZ * Triangles[0].Vertices[i];
					Fragment.Vertices[i] = MapToScreenSpace * MapToPersp * MapToCamera * Triangles[0].Vertices[i];
					Fragment.Vertices[i] = (1.0f / Fragment.Vertices[i].w) * Fragment.Vertices[i];
				}

				scan_buffer_fill_triangle(&ScanBuffer, &Fragment);
				scan_buffer_draw_shape(&Win32BackBuffer, &ScanBuffer, Fragment.Vertices[0].y, Fragment.Vertices[2].y, Triangles[0].Color);

				Color = {1.0f, 1.0f, 1.0f};
				for (u32 i = 0; i < 3; i++) {
					line_draw_dda(&Win32BackBuffer, tmp.xy, Fragment.Vertices[i].xy, Color);
				}
#endif

				StretchDIBits(DeviceContext,
						0, 0, Win32BackBuffer.width, Win32BackBuffer.height,
						0, 0, Win32BackBuffer.width, Win32BackBuffer.height,
						Win32BackBuffer.memory,
						&Win32BackBuffer.Info,
						DIB_RGB_COLORS,
						SRCCOPY);

				LARGE_INTEGER tick_count_after;
				QueryPerformanceCounter(&tick_count_after);

				f32 tick_count_elapsed = (f32)(tick_count_after.QuadPart - tick_count_before.QuadPart);
				time_delta = tick_count_elapsed * time_for_each_tick;
				tick_count_before = tick_count_after;
			}
		}
	}
	return(0);
}
