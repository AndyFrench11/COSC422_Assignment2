//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  FILE NAME: MannequinProgram.cpp
//  
//  Press key '1' to toggle 90 degs model rotation about x-axis on/off.
//  ========================================================================

#include <iostream>
#include <map>
#include <GL/freeglut.h>
#include <IL/il.h>
using namespace std;

#include <assimp/cimport.h>
#include <assimp/types.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "assimp_extras.h"

//----------Globals----------------------------
const aiScene* modelScene = NULL;
const aiScene* animationScene = NULL;
float angle = 0;
aiVector3D scene_min, scene_max, scene_center;
bool modelRotn = true;
std::map<int, int> texIdMap;

//---------Animation Variables-----------------
int tDuration; //Animation duration in ticks.
int currTick = 0; //current tick
float timeStep = 50; //Animation time step = 50 m.sec

//------------Modify the following as needed----------------------
float materialCol[4] = { 0.5, 0.4, 0.3, 1 };   //Default material colour (not used if model's colour is available)
bool replaceCol = true;                       //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = { 0, 50, 50, 1 };         //Default light's position
bool twoSidedLight = false;                    //Change to 'true' to enable two-sided lighting

//-------Loads model data from file and creates a scene object----------
bool loadModel(const char* fileName)
{
    modelScene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
    if(modelScene == NULL) exit(1);
    //printSceneInfo(modelScene);
    //printMeshInfo(modelScene);
    //printTreeInfo(modelScene->mRootNode);
    //printBoneInfo(modelScene);
    //printAnimInfo(modelScene);  //WARNING:  This may generate a lengthy output if the model has animation data
    get_bounding_box(modelScene, &scene_min, &scene_max);
    return true;
}

//-------Loads model data from file and creates a scene object----------
bool loadAnimation(const char* fileName)
{
    animationScene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_Debone);
    tDuration = animationScene->mAnimations[0]->mDuration;
    if(animationScene == NULL) exit(1);
    //printSceneInfo(animationScene);
    //printMeshInfo(animationScene);
    //printTreeInfo(animationScene->mRootNode);
    //printBoneInfo(animationScene);
    printAnimInfo(animationScene);  //WARNING:  This may generate a lengthy output if the model has animation data
    //get_bounding_box(animationScene, &scene_min, &scene_max);
    return true;
}


// ------A recursive function to traverse scene graph and render each mesh----------
void render (const aiScene* sc, const aiNode* nd)
{
    aiMatrix4x4 m = nd->mTransformation;
    aiMesh* mesh;
    aiFace* face;
    aiMaterial* mtl;
    GLuint texId;
    aiColor4D diffuse;
    int meshIndex, materialIndex;

    aiTransposeMatrix4(&m);   //Convert to column-major order
    glPushMatrix();
    glMultMatrixf((float*)&m);   //Multiply by the transformation matrix for this node

    // Draw all meshes assigned to this node
    for (int n = 0; n < nd->mNumMeshes; n++)
    {
        meshIndex = nd->mMeshes[n];          //Get the mesh indices from the current node
        mesh = modelScene->mMeshes[meshIndex];    //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh
        mtl = sc->mMaterials[materialIndex];
        if (replaceCol)
            glColor4fv(materialCol);   //User-defined colour
        else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))  //Get material colour from model
            glColor4f(diffuse.r, diffuse.g, diffuse.b, 1.0);
        else
            glColor4fv(materialCol);   //Default material colour


        //Get the polygons from each mesh and draw them
        for (int k = 0; k < mesh->mNumFaces; k++)
        {
            face = &mesh->mFaces[k];
            GLenum face_mode;

            switch(face->mNumIndices)
            {
                case 1: face_mode = GL_POINTS; break;
                case 2: face_mode = GL_LINES; break;
                case 3: face_mode = GL_TRIANGLES; break;
                default: face_mode = GL_POLYGON; break;
            }

            glBegin(face_mode);

            for(int i = 0; i < face->mNumIndices; i++) {
                int vertexIndex = face->mIndices[i]; 

                if(mesh->HasVertexColors(0))
                    glColor4fv((GLfloat*)&mesh->mColors[0][vertexIndex]);

                //Assign texture coordinates here
                
                if (mesh->HasTextureCoords(0)) {
                    
                    glTexCoord2f (mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y );
                    
                }

                if (mesh->HasNormals())
                    glNormal3fv(&mesh->mNormals[vertexIndex].x);

                glVertex3fv(&mesh->mVertices[vertexIndex].x);
            }

            glEnd();
        }
    }

    // Draw all children
    for (int i = 0; i < nd->mNumChildren; i++)
        render(sc, nd->mChildren[i]);

    glPopMatrix();
}

//--------------------OpenGL initialization------------------------
void initialise()
{
    float ambient[4] = { 0.2, 0.2, 0.2, 1.0 };  //Ambient light
    float white[4] = { 1, 1, 1, 1 };            //Light's colour
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    if (twoSidedLight) glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50);
    glColor4fv(materialCol);
    loadModel("mannequin.fbx"); 
    loadAnimation("run.fbx");
           //<<<-------------Specify input file name here
    //loadGLTextures(scene);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(35, 1, 1.0, 1000.0);
}

void updateNodeMatrices(int tick)
{
    int index;
    aiAnimation* anim = animationScene->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;
    
    for (int i = 0; i < anim->mNumChannels; i++)
    {
        matPos = aiMatrix4x4(); //Identity
        matRot = aiMatrix4x4();
        aiNodeAnim* ndAnim = anim->mChannels[i]; //Channel
        
        if (ndAnim->mNumPositionKeys > 1) index = tick;
        else index = 0;
        aiVector3D posn = (ndAnim->mPositionKeys[index]).mValue;
        matPos.Translation(posn, matPos);
        
        if (ndAnim->mNumRotationKeys > 1) index = tick;
        else index = 0;
        aiQuaternion rotn = (ndAnim->mRotationKeys[index]).mValue;
        matRot3 = rotn.GetMatrix();
        matRot = aiMatrix4x4(matRot3);
    
        matProd = matPos * matRot;
        nd = animationScene->mRootNode->FindNode(ndAnim->mNodeName);
        nd->mTransformation = matProd;
    }
}

void update(int value)
{
    printf("tDuration %i", tDuration);
    if (currTick <= tDuration)
    {
        printf("currTick %i", currTick);
        updateNodeMatrices(currTick);
        glutTimerFunc(timeStep, update, 0);
        currTick++;
    } else {
        currTick = 0;
    }
    
    glutPostRedisplay();
}

//----Timer callback for continuous rotation of the model about y-axis----
//~ void update(int value)
//~ {
    //~ angle++;
    //~ if(angle > 360) angle = 0;
    //~ glutPostRedisplay();
    //~ glutTimerFunc(50, update, 0);
//~ }

//----Keyboard callback to toggle initial model orientation---
void keyboard(unsigned char key, int x, int y)
{
    //if(key == '1') modelRotn = !modelRotn;  //Enable/disable initial model rotation
    //if(key == '2') modelRotn = !modelRotn;  //Enable/disable initial model rotation 
    glutPostRedisplay();
}

void drawFloor()
{
    bool flag = false;

    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for(int x = -5000; x <= 5000; x += 50)
    {
        for(int z = -5000; z <= 5000; z += 50)
        {
            if(flag) glColor3f(0.1, 1.0, 0.3);
            else glColor3f(0.7, 1.0, 0.5);
            glVertex3f(x, 0, z);
            glVertex3f(x, 0, z+50);
            glVertex3f(x+50, 0, z+50);
            glVertex3f(x+50, 0, z);
            flag = !flag;
        }
    }
    glEnd();
}

//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 1, 3, 0, 0, -5, 0, 1, 0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);

    //glRotatef(angle, 0.f, 1.f ,0.f);  //Continuous rotation about the y-axis
    //if(modelRotn) glRotatef(-90, 1, 0, 0);        //First, rotate the model about x-axis if needed.

    // scale the whole asset to fit into our view frustum 
    float tmp = scene_max.x - scene_min.x;
    tmp = aisgl_max(scene_max.y - scene_min.y,tmp);
    tmp = aisgl_max(scene_max.z - scene_min.z,tmp);
    tmp = 1.f / tmp;
    glScalef(tmp, tmp, tmp);

    float xc = (scene_min.x + scene_max.x)*0.5;
    float yc = (scene_min.y + scene_max.y)*0.5;
    float zc = (scene_min.z + scene_max.z)*0.5;
    // center the model
    glTranslatef(-xc, -yc, -zc);
    
    drawFloor();

    render(modelScene, modelScene->mRootNode);
    
    glutSwapBuffers();
}



int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Mannequin Program");
    glutInitContextVersion (4, 2);
    glutInitContextProfile ( GLUT_CORE_PROFILE );

    initialise();
    glutDisplayFunc(display);
    glutTimerFunc(timeStep, update, 0);
    glutKeyboardFunc(keyboard);
    glutMainLoop();

    aiReleaseImport(modelScene);
}

