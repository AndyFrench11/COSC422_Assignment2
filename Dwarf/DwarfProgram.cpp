//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  FILE NAME: DwarfProgram.cpp
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
const aiScene* scene = NULL;
const aiScene* animationScene = NULL;
aiVector3D scene_min, scene_max, scene_center;
std::map<int, int> texIdMap;

//---------Animation Variables-----------------
int tDuration; //Animation duration in ticks.
int currTick = 0; //current tick
float timeStep = 50; //Animation time step = 50 m.sec
bool embeddedAnimation = false;
bool reTargetedAnimation = false;

std::map<string, string> animationRemapping
{
    {"rThigh", "lhip"},
    {"lThigh", "rhip"},
    {"rShin", "lknee"},
    {"lShin", "rknee"},
    {"rFoot", "lankle"},
    {"lFoot", "rankle"},
};

//---------Camera Variables--------------------
float radius = 3, angle=0, look_x, look_y = 0, look_z=0, eye_x = 0, eye_y = 0, eye_z = radius, prev_eye_x = eye_x, prev_eye_z=eye_z;  //Camera parameters

//------------Modify the following as needed----------------------
float materialCol[4] = { 0.9, 0.9, 0.9, 1 };   //Default material colour (not used if model's colour is available)
bool replaceCol = true;                       //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = { 0, 50, 50, 1 };         //Default light's position
bool twoSidedLight = false;                    //Change to 'true' to enable two-sided lighting
float shadowMatrix[16] = 
{ 
    50,0,0,0, 
    0,0,-50,-1, 
    0,0,50,0, 
    0,0,0,50 };

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
    scene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
    if(scene == NULL) exit(1);
    //printSceneInfo(scene);
    //printMeshInfo(scene);
    //printTreeInfo(scene->mRootNode);
    //printBoneInfo(scene);
    //printAnimInfo(scene);  //WARNING:  This may generate a lengthy output if the model has animation data
    
    initData = new meshInit[scene->mNumMeshes];
    
    for (int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[i];
        (initData + i)->mNumVertices = mesh->mNumVertices;
        (initData + i)->mVertices = new aiVector3D[mesh->mNumVertices];
        (initData + i)->mNormals = new aiVector3D[mesh->mNumVertices];
        
        for (int j = 0; j < mesh->mNumVertices; j++)
        {
            (initData + i)->mVertices[j] = mesh->mVertices[j];
            (initData + i)->mNormals[j] = mesh->mNormals[j];
        }
    }
    
    get_bounding_box(scene, &scene_min, &scene_max);
    return true;
}

//-------Loads model data from file and creates a scene object----------
bool loadAnimation(const char* fileName)
{
    animationScene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_Debone);
    //tDuration = animationScene->mAnimations[0]->mDuration;
    if(animationScene == NULL) exit(1);
    //printSceneInfo(animationScene);
    //printMeshInfo(animationScene);
    //printTreeInfo(animationScene->mRootNode);
    //printBoneInfo(animationScene);
    //printAnimInfo(animationScene);  //WARNING:  This may generate a lengthy output if the model has animation data
    return true;
}

//-------------Loads texture files using DevIL library-------------------------------
void loadGLTextures(const aiScene* scene)
{
    /* initialization of DevIL */
    ilInit();
    if (scene->HasTextures())
    {
        std::cout << "Support for meshes with embedded textures is not implemented" << endl;
        return;
    }

    /* scan scene's materials for textures */
    /* Simplified version: Retrieves only the first texture with index 0 if present*/
    for (unsigned int m = 0; m < scene->mNumMaterials; ++m)
    {
        aiString path;  // filename

        if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
        {
            glEnable(GL_TEXTURE_2D);
            ILuint imageId;
            GLuint texId;
            ilGenImages(1, &imageId);
            glGenTextures(1, &texId);
            texIdMap[m] = texId;   //store tex ID against material id in a hash map
            ilBindImage(imageId); /* Binding of DevIL image name */
            ilEnable(IL_ORIGIN_SET);
            ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
            if (ilLoadImage((ILstring)path.data))   //if success
            {
                /* Convert image to RGBA */
                ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

                /* Create and load textures to OpenGL */
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
                    ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    ilGetData());
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                cout << "Texture:" << path.data << " successfully loaded." << endl;
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            }
            else
            {
                cout << "Couldn't load Image: " << path.data << endl;
            }
        }
    }  //loop for material

}

// ------A recursive function to traverse scene graph and render each mesh----------
void render (const aiScene* sc, const aiNode* nd, bool shadow)
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
        mesh = scene->mMeshes[meshIndex];    //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh
        mtl = sc->mMaterials[materialIndex];
        
        if(mesh->HasTextureCoords(0)) {
            glEnable(GL_TEXTURE);
            glBindTexture(GL_TEXTURE_2D, texIdMap[materialIndex]);
        }
        
        if(shadow) {
            glColor4f(0, 0, 0, 1.0);
        }
        else if (replaceCol) 
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
                    
                if(mesh->HasTextureCoords(0)) 
                    glTexCoord2f (mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y );

                if (mesh->HasNormals())
                    glNormal3fv(&mesh->mNormals[vertexIndex].x);

                glVertex3fv(&mesh->mVertices[vertexIndex].x);
            }

            glEnd();
        }
    }

    // Draw all children
    for (int i = 0; i < nd->mNumChildren; i++)
        render(sc, nd->mChildren[i], shadow);

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
    //glColor4fv(materialCol);
    loadModel("dwarf.x"); //<<<-------------Specify input file name here
    loadAnimation("avatar_walk.bvh");
    loadGLTextures(scene);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(35, 1, 1.0, 1000.0);
}

void transformVertices()
{
    
    for (int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[i];
        
        for (int j = 0; j < mesh->mNumBones; j++)
        {
            aiBone* bone = mesh->mBones[j];
            aiMatrix4x4 limatrix = bone->mOffsetMatrix;
            aiNode* node = scene->mRootNode->FindNode(bone->mName);
            aiMatrix4x4 mimatrix = limatrix;
            
            while(node->mParent != NULL) {
                aiMatrix4x4 qamatrix = node->mTransformation;
                mimatrix = qamatrix * mimatrix;
                node = node->mParent;
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
    aiAnimation* anim = scene->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;
    
    for (int i = 0; i < anim->mNumChannels; i++)
    {
        matPos = aiMatrix4x4(); //Identity
        matRot = aiMatrix4x4();
        aiNodeAnim* channel = anim->mChannels[i]; //Channel
        
        if(reTargetedAnimation) {
            //aiAnimation* newAnim = animationScene->mAnimations[0];
            for (int j = 0; j < scene->mAnimations[0]->mNumChannels; j++)
            {
                if(scene->mAnimations[0]->mChannels[j]->mNodeName.data == animationRemapping[channel->mNodeName.data]) {
                    channel = scene->mAnimations[0]->mChannels[j];
                    break;
                }
            }
        }
        
        aiVector3D posn;
        
        // Position Keys
        if (channel->mNumPositionKeys > 1) {
            for (int positionIndex = 0; positionIndex < channel->mNumPositionKeys; positionIndex++)
            {
                if(tick < channel->mPositionKeys[positionIndex].mTime) {
                    aiVector3D  pos1 = (channel->mPositionKeys[positionIndex-1]).mValue;
                    aiVector3D  pos2 = (channel->mPositionKeys[positionIndex]).mValue;
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
        
         if(reTargetedAnimation) {
             anim = animationScene->mAnimations[0];
         }
         
         channel = anim->mChannels[i]; //Channel
        
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
        nd = scene->mRootNode->FindNode(channel->mNodeName);
        
        if(reTargetedAnimation) {
             nd = scene->mRootNode->FindNode(aiString(animationRemapping[channel->mNodeName.data]));
        }
        if(nd != NULL) {
            nd->mTransformation = matProd;
        }
        
    }
    transformVertices();
}

void update(int value)
{
    if(embeddedAnimation) {
    
        tDuration = scene->mAnimations[0]->mDuration;
        if (currTick < tDuration)
        {
            updateNodeMatrices(currTick);
            glutTimerFunc(timeStep, update, 0);
            currTick++;
        } 
        else {
            currTick = 0;
            glutTimerFunc(timeStep, update, 0);
            embeddedAnimation = false;
        }
    } else if(reTargetedAnimation) {
        tDuration = animationScene->mAnimations[0]->mDuration;
        if (currTick < tDuration)
        {
            updateNodeMatrices(currTick);
            glutTimerFunc(timeStep, update, 0);
            currTick++;
        } 
        else {
            currTick = 0;
            glutTimerFunc(timeStep, update, 0);
            reTargetedAnimation = false;
        }
    } else {
        glutTimerFunc(timeStep, update, 0);
    }
    glutPostRedisplay();
}

//----Keyboard callback to toggle initial model orientation---
void keyboard(unsigned char key, int x, int y)
{
    if(key == '1') embeddedAnimation = !embeddedAnimation; 
    if(key == '2') reTargetedAnimation = !reTargetedAnimation; 
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
            if(flag) glColor3f(0.6, 1.0, 0.8);
            else glColor3f(0.8, 1.0, 0.6);
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
    gluLookAt(eye_x, eye_y, eye_z,  look_x, look_y, look_z,   0, 1, 0);
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
    
    glPushMatrix();
    drawFloor();
    glPopMatrix();
    
    glDisable(GL_LIGHTING); //Shadow
    glPushMatrix();
    glTranslatef(0, 0.1, 0);
    glMultMatrixf(shadowMatrix);
    glTranslatef(-xc, -yc, -zc);
    render(scene, scene->mRootNode, true);
    glPopMatrix();

    glEnable(GL_LIGHTING);
    glPushMatrix();
    render(scene, scene->mRootNode, false);
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
    glutCreateWindow("Dwarf Program");
    glutInitContextVersion (4, 2);
    glutInitContextProfile ( GLUT_CORE_PROFILE );

    initialise();
    glutDisplayFunc(display);
    glutTimerFunc(timeStep, update, 0);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();

    aiReleaseImport(scene);
}

