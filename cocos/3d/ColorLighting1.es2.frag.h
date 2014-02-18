static const char* ColorLighting1 = STRINGIFY(

varying lowp vec4 DestinationColor;
//varying mediump vec2 TextureCoordOut;

//uniform sampler2D Sampler;

void main(void)
{
    gl_FragColor =  DestinationColor;
}
);
