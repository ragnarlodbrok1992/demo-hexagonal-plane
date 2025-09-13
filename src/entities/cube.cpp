#ifndef _H_CUBE
#define _H_CUBE

// TODO(moliwa): Make primitives crossplatform
#include "../win32_primitives.cpp"

// TODO(moliwa): Create new entity/struct Cube, not just an array of vertices...
const int DEFAULT_CUBE_VERTICES = 8;

// TODO(moliwa): Vertices and indices should live together in harmony
unsigned short cubeIndices[] = {
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};

// TODO(moliwa): Can we have a function of runtime and constexpr at the same time?
DirectX::XMFLOAT4 getRandomColor() {
  DirectX::XMFLOAT4 temp = {}; 
  temp.x = (float)rand() / RAND_MAX;
  temp.y = (float)rand() / RAND_MAX;
  temp.z = (float)rand() / RAND_MAX;
  temp.w = 1.0f;
  return temp;
}

Vertex* createDefaultCube() {
  Vertex* cubeVertices = (Vertex*)malloc(DEFAULT_CUBE_VERTICES * sizeof(Vertex));
  // TODO(moliwa): Check for failed allocation
  cubeVertices[0].pos   = DirectX::XMFLOAT3(-0.5f, 0.5f, -0.5f);
  cubeVertices[0].color = getRandomColor();

  cubeVertices[1].pos   = DirectX::XMFLOAT3(0.5f, 0.5f, -0.5f);
  cubeVertices[1].color = getRandomColor();

  cubeVertices[2].pos   = DirectX::XMFLOAT3(0.5f, -0.5f, -0.5f);
  cubeVertices[2].color = getRandomColor();

  cubeVertices[3].pos   = DirectX::XMFLOAT3(-0.5f, -0.5f, -0.5f);
  cubeVertices[3].color = getRandomColor();

  cubeVertices[4].pos   = DirectX::XMFLOAT3(-0.5f, 0.5f, 0.5f);
  cubeVertices[4].color = getRandomColor();

  cubeVertices[5].pos   = DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f);
  cubeVertices[5].color = getRandomColor();

  cubeVertices[6].pos   = DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f);
  cubeVertices[6].color = getRandomColor();

  cubeVertices[7].pos   = DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f);
  cubeVertices[7].color = getRandomColor();

  return cubeVertices;
}

#endif /* _H_CUBE */
