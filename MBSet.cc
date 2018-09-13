// Calculate and display the Mandelbrot set
// ECE6122 final project, Spring 2017
// Yuanda Zhu

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include <GL/glut.h>
#include <GL/glext.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "complex.h"

#include <vector>

using namespace std;

// Min and max complex plane values
Complex  minC(-2.0, -1.2);
Complex  maxC( 1.0, 1.8);
//int      maxIt = 2000;     // Max iterations for the set computations
#define maxIt 2000
#define window_size 512
int nThreads = 16;
bool* localSense;
bool globalSense;
int p0 = nThreads + 1;
int count;

// The mutex and condition variables allow the main thread to
// know when all helper threads are completed.
pthread_mutex_t startCountMutex;
pthread_mutex_t exitMutex;
pthread_mutex_t countMutex;
//pthread_mutex_t elementCountMutex;
pthread_cond_t exitCond;


Complex* c = new Complex[window_size * window_size];
int matrix_iterations[window_size * window_size];

float color_r[maxIt];
float color_g[maxIt];
float color_b[maxIt];

class my_windows
{
  public:
    float min_x, min_y, max_x, max_y;
    my_windows(float x1, float y1, float x2, float y2)
    {
      min_x = x1;
      min_y = y1;
      max_x = x2;
      max_y = y2;
    }
};

vector<my_windows> window_vector;

// Mouse operation global variables
bool mouse_click;
float dd; // difference in selection box; the larger side


struct two_points
{
  float x,y;
  two_points()
  {
    x = 0.0;
    y = 0.0;
  }
};

two_points point1, point2;


struct RGB_values
{
  float r, g, b;
};

RGB_values hue_to_RGB(float k)
{
  RGB_values colors;
  float r, g, b;
  int kk = (int)floor(k/60.0f);
  float rotk = k/60.0f - kk;
  float rtz = 0;
  float s4 = 1 - rotk;
  float ana = 1 - (1 - rotk);

  switch (kk % 6)
  {
    case 0:
    {
      r = 1;
      g = ana;
      b = rtz;
      break;
    }
    case 1:
    {
      r = s4;
      g = 1;
      b = rtz;
      break;
    }
    case 2:
    {
      r = rtz;
      g = 1;
      b = rtz;
      break;
    }
    case 3:
    {
      r = rtz;
      g = s4;
      b = 1;
      break;
    }
    case 4:
    {
      r = ana;
      g = rtz;
      b = 1;
      break;
    }
    case 5:
    {
      r = 1;
      g = rtz;
      b = s4;
      break;
    }
  }
  colors.r = r;
  colors.g = g;
  colors.b = b;
  return colors;
}

void generate_colors(int iterations)
{
  RGB_values colors;
  float k;
  for (int kk = 0; kk < iterations; kk++)
  {
    k = (kk % 36 ) * 10;
    colors = hue_to_RGB(k);
    color_r[kk] = colors.r;
    color_g[kk] = colors.g;
    color_b[kk] = colors.b;
  }
}

int FetchAndDecrementCount()
{
  pthread_mutex_lock(&countMutex);
  int myCount = count;
  count--;
  pthread_mutex_unlock(&countMutex);
  return myCount;
}

// Call MyBarrier_Init once in main
void MyBarrier_Init()// you will likely need some parameters)
{
  count = p0;
  pthread_mutex_init(&countMutex,0);
  localSense = new bool[p0];
  for (int k=0; k<p0; ++k)
  {
    localSense[k] = true;
  }
  globalSense = true;
}

void MyBarrier(int myId)
{
  localSense[myId] = !localSense[myId];
  //cout << "barrier " << myId << endl;
  if (FetchAndDecrementCount() == 1)
  {
    count = p0;
    globalSense = localSense[myId];
    cout << "Barrier free at " << myId << endl;
  }
  else
  {
    while (globalSense != localSense[myId]){} // spin
  }
}

void zoom_new_box(void)
{
  glColor3f(1.0, 0.0, 0.0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glBegin(GL_POLYGON);
  glVertex2f(point1.x, point1.y);
  glVertex2f(point1.x, point2.y);
  glVertex2f(point2.x, point2.y);
  glVertex2f(point2.x, point1.y);
  glEnd();
}


void display(void)
{ // Your OpenGL display code here
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glClearColor(1.0, 1.0, 1.0, 1.0);
  glLoadIdentity();
  
  // display MBSets here
  glBegin(GL_POINTS);
  int iteration_ind;
  for (int k = 0; k < window_size; k++)
  {
    for (int kk = 0; kk < window_size; kk++)
    {
      iteration_ind = matrix_iterations[k*window_size+kk];
      if (iteration_ind == maxIt)
      {
        glColor3f(0.0, 0.0, 0.0);
        glVertex2d(k,kk);
      }
      else if (iteration_ind > 0)
      {
        glColor3f(color_r[iteration_ind], color_g[iteration_ind], color_b[iteration_ind]);
        glVertex2d(k,kk);
      }
    }
  }
  glEnd();

  if (mouse_click == 1)
  {
    zoom_new_box();
  }

  glutSwapBuffers();
}

void calculate_c_values(void)
{
  for (int k = 0; k < window_size; k++)
  {
    for (int kk = 0; kk < window_size; kk++)
    {
      double real_part = (maxC.real - minC.real) * k / window_size;
      double imag_part = (maxC.imag - minC.imag) * kk / window_size;
      c[k * window_size + kk] = minC + Complex(real_part, imag_part);
    }
  }
}

void* my_threads(void* v)
{
  //
  int myId = (unsigned long)v;
  int rows_per_thread = window_size / nThreads;
  for (int k = 0; k < rows_per_thread; k++)
  {
    int current_row = myId * rows_per_thread + k;
    for (int kk = 0; kk < window_size; kk++)
    {
      Complex temp = c[current_row * window_size + kk];
      matrix_iterations[current_row * window_size + kk] = 0;

      while (matrix_iterations[current_row * window_size + kk] < maxIt && temp.Mag2() < 4.0)
      {
        temp = temp * temp + c[current_row * window_size + kk];
        matrix_iterations[current_row * window_size + kk]++;
      }
    }
  }
}


void create_threads(void)
{
  // Initialize c values
  calculate_c_values();

  // Create barrier
  MyBarrier_Init();
  // Create 16 threads
  pthread_mutex_init(&exitMutex,0);
  pthread_mutex_init(&startCountMutex,0);
  pthread_cond_init(&exitCond, 0);

  pthread_t threads[nThreads];
  
  for (int th=0; th<nThreads; th++)
  {
    pthread_create(&threads[th], 0, my_threads, (void *)th);
  }
}

void init()
{ 
  create_threads();  

  // Your OpenGL initialization code here
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glShadeModel(GL_FLAT);
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, window_size, window_size, 0);
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_LINE_SMOOTH);
  glLoadIdentity();
}

void mouse(int button, int state, int x, int y)
{ // Your mouse click processing here
  // state == 0 means pressed, state != 0 means released
  // Note that the x and y coordinates passed in are in
  // PIXELS, with y = 0 at the top.

  if (button == GLUT_LEFT_BUTTON && state == 0)
  {
    mouse_click = 1;
    point1.x = x;
    point1.y = y;
    point2.x = x;
    point2.y = y;
  }

  if (button == GLUT_LEFT_BUTTON && state == 1)
  {
    window_vector.push_back(my_windows(minC.real, minC.imag, maxC.real, maxC.imag));

    // Conditions where the mouse stops
    if (x > point1.x && y > point1.y)
    {
      point2.x = point1.x + dd;
      point2.y = point1.y + dd;
      for (int k = 0; k < window_size; k++)
      {
        for (int kk = 0; kk < window_size; kk++)
        {
          if (k == point1.x && kk == point1.y)
          {
            minC.real = c[k * window_size + kk].real;
            minC.imag = c[k * window_size + kk].imag;
          }
          if (k == point2.x && kk == point2.y)
          {
            maxC.real = c[k * window_size + kk].real;
            maxC.imag = c[k * window_size + kk].imag;
          }
        }
      }
    }

    if (x > point1.x && y < point1.y)
    {
      point2.x = point1.x + dd;
      point2.y = point1.y - dd;
      for (int k = 0; k < window_size; k++)
      {
        for (int kk = 0; kk < window_size; kk++)
        {
          if (k == point1.x && kk == point2.y)
          {
            minC.real = c[k * window_size + kk].real;
            minC.imag = c[k * window_size + kk].imag;
          }
          if (k == point2.x && kk == point1.y)
          {
            maxC.real = c[k * window_size + kk].real;
            maxC.imag = c[k * window_size + kk].imag;
          }
        }
      }
    }

    if (x < point1.x && y > point1.y)
    {
      point2.x = point1.x - dd;
      point2.y = point1.y + dd;
      for (int k = 0; k < window_size; k++)
      {
        for (int kk = 0; kk < window_size; kk++)
        {
          if (k == point2.x && kk == point1.y)
          {
            minC.real = c[k * window_size + kk].real;
            minC.imag = c[k * window_size + kk].imag;
          }
          if (k == point1.x && kk == point2.y)
          {
            maxC.real = c[k * window_size + kk].real;
            maxC.imag = c[k * window_size + kk].imag;
          }
        }
      }
    }

    if (x < point1.x && y < point1.y)
    {
      point2.x = point1.x - dd;
      point2.y = point1.y - dd;
      for (int k = 0; k < window_size; k++)
      {
        for (int kk = 0; kk < window_size; kk++)
        {
          if (k == point2.x && kk == point2.y)
          {
            minC.real = c[k * window_size + kk].real;
            minC.imag = c[k * window_size + kk].imag;
          }
          if (k == point1.x && kk == point1.y)
          {
            maxC.real = c[k * window_size + kk].real;
            maxC.imag = c[k * window_size + kk].imag;
          }
        }
      }
    }

    mouse_click = 0;
    create_threads();
    glutPostRedisplay();

  }
}

void motion(int x, int y)
{ // Your mouse motion here, x and y coordinates are as above
  if (abs(x - point1.x) >= abs(y - point1.y))
  {
    dd = abs(x - point1.x);
  }
  else
  {
    dd = abs(y - point1.y);
  }

  if (x > point1.x)
  {
    point2.x = point1.x + dd;
  }
  else
  {
    point2.x = point1.x - dd;
  }

  if (y > point1.y)
  {
    point2.y = point1.y + dd;
  }
  else
  {
    point2.y = point1.y - dd;
  }

  glutPostRedisplay();
  
}

void keyboard(unsigned char c, int x, int y)
{ // Your keyboard processing here
  if (c == 'b' || c == 'B')
  {
    if (window_vector.size() > 0)
    {
      my_windows notail = window_vector.back();
      window_vector.pop_back();
      minC.real = notail.min_x;
      minC.imag = notail.min_y;
      maxC.real = notail.max_x;
      maxC.imag = notail.max_y;
      create_threads();
      glutPostRedisplay();
    }
  }
}



int main(int argc, char** argv)
{
  // Initialize OpenGL, but only on the "master" thread or process.
  // See the assignment writeup to determine which is "master" 
  // and which is slave.

  // Initialize color arrays
  generate_colors(maxIt);  

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(window_size, window_size);
  glutInitWindowPosition(100, 100);
  glutCreateWindow("MBSet");
  
  init();

  glutDisplayFunc(display);
  glutIdleFunc(display);
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutMainLoop();

  return 0;
}

