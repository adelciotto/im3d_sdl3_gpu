static const float ANTIALIASING = 2.0;

#if defined(VERTEX_SHADER)
struct Vertex_Data {
  float3 position;
  float  size;
  uint   color;
};

StructuredBuffer<Vertex_Data> Data_Buffer : register(t0, space0);

struct Input {
  float4 position : TEXCOORD0;
  uint   vertex_id : SV_VertexID;
  uint   instance_id : SV_InstanceID;
};

struct Output {
#if defined(PRIMITIVE_KIND_POINTS)
  noperspective float2 texcoord : TEXCOORD0;
#elif defined(PRIMITIVE_KIND_LINES)
  noperspective float edge_distance : TEXCOORD0;
#endif
  noperspective float size : TEXCOORD1;
  float4              color : TEXCOORD2;
  float4              position : SV_Position;
};

cbuffer Uniform_Block : register(b0, space1) {
  float4x4 world_to_clip_transform : packoffset(c0);
  float2   resolution : packoffset(c4);
}

float4 uint_to_rgba(uint u) {
  float4 color = float4(0.0, 0.0, 0.0, 0.0);
  color.r      = float((u & 0xff000000u) >> 24u) / 255.0;
  color.g      = float((u & 0x00ff0000u) >> 16u) / 255.0;
  color.b      = float((u & 0x0000ff00u) >> 8u) / 255.0;
  color.a      = float((u & 0x000000ffu) >> 0u) / 255.0;
  return color;
}

Output main(Input input) {
  Output output;

#if defined(PRIMITIVE_KIND_POINTS)
  Vertex_Data vertex_data = Data_Buffer[input.instance_id];

  output.size  = max(vertex_data.size, ANTIALIASING);
  output.color = uint_to_rgba(vertex_data.color);
  output.color.a *= smoothstep(0.0, 1.0, output.size / ANTIALIASING);

  output.position = mul(world_to_clip_transform, float4(vertex_data.position, 1.0));
  float2 scale    = 1.0 / resolution * output.size;
  output.position.xy += input.position.xy * scale * output.position.w;

  output.texcoord = input.position.xy * 0.5 + 0.5;

#elif defined(PRIMITIVE_KIND_LINES)
  uint        instance_id_0 = input.instance_id * 2;
  uint        instance_id_1 = instance_id_0 + 1;
  uint        instance_id   = (input.vertex_id % 2 == 0) ? instance_id_0 : instance_id_1;
  Vertex_Data vertex_data   = Data_Buffer[instance_id];

  output.size  = max(vertex_data.size, ANTIALIASING);
  output.color = uint_to_rgba(vertex_data.color);
  output.color.a *= smoothstep(0.0, 1.0, output.size / ANTIALIASING);
  output.edge_distance = output.size * input.position.y;

  float4 pos_0     = mul(world_to_clip_transform, float4(Data_Buffer[instance_id_0].position, 1.0));
  float4 pos_1     = mul(world_to_clip_transform, float4(Data_Buffer[instance_id_1].position, 1.0));
  float2 direction = (pos_0.xy / pos_0.w) - (pos_1.xy / pos_1.w);
  direction        = normalize(float2(direction.x, direction.y * resolution.y / resolution.x));
  float2 tng       = float2(-direction.y, direction.x) * output.size / resolution;

  output.position = (input.vertex_id % 2 == 0) ? pos_0 : pos_1;
  output.position.xy += tng * input.position.y * output.position.w;

#elif defined(PRIMITIVE_KIND_TRIANGLES)
  Vertex_Data vertex_data = Data_Buffer[input.instance_id * 3 + input.vertex_id];
  output.color            = uint_to_rgba(vertex_data.color);
  output.position         = mul(world_to_clip_transform, float4(vertex_data.position, 1.0));
#endif

  return output;
}
#endif

#if defined(FRAGMENT_SHADER)
struct Input {
#if defined(PRIMITIVE_KIND_POINTS)
  noperspective float2 texcoord : TEXCOORD0;
#elif defined(PRIMITIVE_KIND_LINES)
  noperspective float edge_distance : TEXCOORD0;
#endif
  noperspective float size : TEXCOORD1;
  float4              color : TEXCOORD2;
};

float4 main(Input input) : SV_Target0 {
  float4 result = input.color;

#if defined(PRIMITIVE_KIND_LINES)
  float d = abs(input.edge_distance) / input.size;
  d       = smoothstep(1.0, 1.0 - (ANTIALIASING / input.size), d);
  result.a *= d;
#elif defined(PRIMITIVE_KIND_POINTS)
  float d = length(input.texcoord - 0.5);
  d       = smoothstep(0.5, 0.5 - (ANTIALIASING / input.size), d);
  result.a *= d;
#endif

  return result;
}
#endif
