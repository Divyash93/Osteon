class_name OsteonBatchNode
extends Node2D
## Internal child of Osteon2DSprite: draws one batch of triangles (one atlas page + blend mode).

var batch: OsteonMeshBuilder.Batch = null
var texture: Texture2D = null


func _draw() -> void:
	if batch == null or batch.indices.is_empty():
		return
	var tex_rid := texture.get_rid() if texture != null else RID()
	RenderingServer.canvas_item_add_triangle_array(get_canvas_item(), batch.indices, batch.points, batch.colors, batch.uvs,
			PackedInt32Array(), PackedFloat32Array(), tex_rid)
