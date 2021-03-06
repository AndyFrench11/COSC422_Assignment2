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
aiVector3D scene_min, scene_max, scene_center;
bool modelRotn = true;
std::map<int, int> texIdMap;

//---------Animation Variables-----------------
int tDuration; //Animation duration in ticks.
int currTick = 0; //current tick
float timeStep = 50; //Animation time step = 50 m.sec


//---------Camera Variables--------------------
float radius = 3, angle=0, look_x, look_y = 0, look_z=0, eye_x = 0, eye_y = 0, eye_z = radius, prev_eye_x = eye_x, prev_eye_z=eye_z;  //Camera parameters

//---------Floor Variables---------------------
int z_floor_close = -5000, z_floor_far = 5000;
int floor_shift = 1;

//---------Model Position Variables---------------------
int z_model = 0;

//------------Modify the following as needed----------------------
float materialCol[4] = { 0.5, 0.4, 0.3, 1 };   //Default material colour (not used if model's colour is available)
bool replaceCol = true;                       //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = { 0, 50, 50, 1 };         //Default light's position
bool twoSidedLight = false;                    //Change to 'true' to enable two-sided lighting

struct meshInit
{
    int mNumVertices;
    aiVector3D* mVertices;
    aiVector3D* mNormals;
};

meshInit* initData;

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
    
    initData = new meshInit[modelScene->mNumMeshes];
    
    for (int i = 0; i < modelScene->mNumMeshes; i++)
    {
        aiMesh* mesh = modelScene->mMeshes[i];
        (initData + i)->mNumVertices = mesh->mNumVertices;
        (initData + i)->mVertices = new aiVector3D[mesh->mNumVertices];
        (initData + i)->mNormals = new aiVector3D[mesh->mNumVertices];
        
        for (int j = 0; j < mesh->mNumVertices; j++)
        {
            (initData + i)->mVertices[j] = mesh->mVertices[j];
            (initData + i)->mNormals[j] = mesh->mNormals[j];
        }
        
    }
    
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

void transformVertices()
{
    
    for (int i = 0; i < modelScene->mNumMeshes; i++)
    {
        aiMesh* mesh = modelScene->mMeshes[i];
        for (int j = 0; j < mesh->mNumBones; j++)
        {
            aiBone* bone = mesh->mBones[j];
            aiMatrix4x4 limatrix = bone->mOffsetMatrix;
            aiNode* node = animationScene->mRootNode->FindNode(bone->mName);
            aiMatrix4x4 mimatrix = limatrix;
            
            while(node->mParent != NULL) {
                if(node->mName != aiString("free3dmodel_skeleton")) {
                    aiMatrix4x4 qamatrix = node->mTransformation;
                    mimatrix = qamatrix * mimatrix;
                    node = node->mParent;
                } else {
                    node = node->mParent;
                }
            }
            //Form the normal matrix.
            aiMatrix4x4 normalMatrix = mimatrix;
            normalMatrix.Inverse().Transpose();
            
            for(int k = 0; k < bone->mNumWeights; k++)
            {
                aiVertexWeight weight = bone->mWeights[k];
                int vid = weight.mVertexId;
                
                aiVector3D vert = (initData + i)->mVertices[vid];
                aiVector3D norm = (initData + i)->mNormals[vid];
                
                //Make a 3x3 of the matrix and multiply it by the 3d vector
                mesh->mVertices[vid] = aiMatrix3x3(mimatrix) * vert + aiVector3D(mimatrix.a4, mimatrix.b4, mimatrix.c4);
                mesh->mNormals[vid] = aiMatrix3x3(normalMatrix) * norm + aiVector3D(normalMatrix.a4, normalMatrix.b4, normalMatrix.c4);
            }
        }
    }
}

void updateNodeMatrices(int tick)
{
    aiAnimation* anim = animationScene->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;
    
    for (int i = 0; i < anim->mNumChannels; i++)
    {
        matPos = aiMatrix4x4(); //Identity
        matRot = aiMatrix4x4();
        aiNodeAnim* channel = anim->mChannels[i]; //Channel
        aiVector3D posn;
        
        // Position Keys
        if (channel->mNumPositionKeys > 1) {
            for (int positionIndex = 0; positionIndex < channel->mNumPositionKeys; positionIndex++)
            {
                if(tick < channel->mPositionKeys[positionIndex].mTime) {
                    aiVector3D   pos1 = (channel->mPositionKeys[positionIndex-1]).mValue;
                    aiVector3D   pos2 = (channel->mPositionKeys[positionIndex]).mValue;
                    double time1 = (channel->mPositionKeys[positionIndex-1]).mTime;
                    double time2 = (channel->mPositionKeys[positionIndex]).mTime;
                    float factor = (tick-time1)/(time2-time1);
                    posn = pos2 - pos1;
                    posn = pos1 + (factor * posn);
                    break;
                } else if(tick == channel->mPositionKeys[positionIndex].mTime) {
                    posn = (channel->mPositionKeys[positionIndex]).mValue;
                    break;
                }
            }
            matPos.Translation(posn, matPos);
        } else {
            posn = (channel->mPositionKeys[0]).mValue;
            matPos.Translation(posn, matPos);
        }
        
        aiQuaternion rotn;
        
        // Rotation Keys
        if (channel->mNumRotationKeys > 1) {
        
            for (int rotationIndex = 0; rotationIndex < channel->mNumRotationKeys; rotationIndex++)
            {
                if(tick < channel->mRotationKeys[rotationIndex].mTime) {
                    aiQuaternion rotn1 = (channel->mRotationKeys[rotationIndex-1]).mValue;
                    aiQuaternion rotn2 = (channel->mRotationKeys[rotationIndex]).mValue;
                    double time1 = (channel->mRotationKeys[rotationIndex-1]).mTime;
                    double time2 = (channel->mRotationKeys[rotationIndex]).mTime;
                    float factor = (tick-time1)/(time2-time1);
                    rotn.Interpolate(rotn, rotn1, rotn2, factor);
                    break;
                } else if(tick == channel->mRotationKeys[rotationIndex].mTime) {
                    rotn = (channel->mRotationKeys[rotationIndex]).mValue; 
                    break;
                }
            }
            matRot3 = rotn.GetMatrix();
            matRot = aiMatrix4x4(matRot3);
        } else {
            rotn = (channel->mRotationKeys[0]).mValue;
            matRot3 = rotn.GetMatrix();
            matRot = aiMatrix4x4(matRot3);
        }
        
        matProd = matPos * matRot;
        nd = animationScene->mRootNode->FindNode(channel->mNodeName);
        nd->mTransformation = matProd;
    }
    transformVertices();
}

void update(int value)
{
    tDuration = animationScene->mAnimations[0]->mDuration;
    if (currTick < tDuration)
    {
        updateNodeMatrices(currTick);
        glutTimerFunc(timeStep, update, 0);
        currTick++;
    } 
    else {
        currTick = 0;
        updateNodeMatrices(currTick);
        glutTimerFunc(timeStep, update, 0);
        currTick++;
    }
    
    z_model += 50;
  
    if(z_model > (2500 * floor_shift)) {
        floor_shift++;
        z_floor_far += 7500;
        
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
     //if(key == '1') embeddedAnimation = !embeddedAnimation; 
    //if(key == '2') modelRotn = !modelRotn;  //Enable/disable initial model rotation 
    glutPostRedisplay();
}

void drawFloor()
{
    bool flag = true;

    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for(int x = -5000; x <= 5000; x += 50)
    {
        for(int z = z_floor_close; z <= z_floor_far; z += 50)
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
    gluLookAt(eye_x, eye_y, eye_z + (z_model * 0.00165) ,  look_x, look_y, look_z + (z_model * 0.00165),  0, 1, 0);
    //gluLookAt(eye_x, eye_y, eye_z,  look_x, look_y, look_z,   0, 1, 0);
    
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
    
    //glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glScalef(2, 1, 2);
    drawFloor();
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(0, 0, z_model);
    glRotatef(-90, 1.0f, 0 ,0);  
    render(modelScene, modelScene->mRootNode);
    glPopMatrix();
    
    glutSwapBuffers();
}

void special(int key, int x, int y)
{   
    if(key == GLUT_KEY_LEFT) angle -= 0.1;  //Change direction
    else if(key == GLUT_KEY_RIGHT) angle += 0.1;
    else if(key == GLUT_KEY_DOWN) radius += 0.1;
    else if(key == GLUT_KEY_UP) 
    {
        if(radius > 1.75) {
            radius -= 0.1;
        }
    }
    
    eye_x = radius * sin(angle);
    eye_z = radius * cos(angle);
    
    glutPostRedisplay();
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
    glutSpecialFunc(special);
    glutMainLoop();

    aiReleaseImport(modelScene);
}

