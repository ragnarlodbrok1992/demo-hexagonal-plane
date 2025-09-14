#ifndef _H_HEXCUBE
#define _H_HEXCUBE

#include "../win32_primitives.cpp"

const int DEFAULT_HEXCUBE_VERTICES = 12;

// TODO(ragnar): Get to drawing and prepare indices values
unsigned short cubeIndices[] = {};

// TODO(moliwa): Can we have a function of runtime and constexpr at the same time?
DirectX::XMFLOAT4 getRandomColor() {
  DirectX::XMFLOAT4 temp = {}; 
  temp.x = (float)rand() / RAND_MAX;
  temp.y = (float)rand() / RAND_MAX;
  temp.z = (float)rand() / RAND_MAX;
  temp.w = 1.0f;
  return temp;
}

// TODO(ragnar): Use some python to calculate the values by hand for now
Vertex* createDefaultHexcube() {
  Vertex* hexcubeVertices = (Vertex*) malloc(DEFAULT_HEXCUBE_VERTICES * sizeof(Vertex));

  return hexcubeVertices;
}

#endif /* _H_HEXCUBE */
