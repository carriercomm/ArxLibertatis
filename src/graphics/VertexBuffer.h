
#ifndef ARX_GRAPHICS_VERTEXBUFFER_H
#define ARX_GRAPHICS_VERTEXBUFFER_H

#include <algorithm>

#include "graphics/Renderer.h"
#include "platform/Platform.h"
#include "platform/Flags.h"

enum BufferFlag {
	DiscardContents = (1<<0),
	NoOverwrite     = (1<<1)
};
DECLARE_FLAGS(BufferFlag, BufferFlags);
DECLARE_FLAGS_OPERATORS(BufferFlags);

template <class Vertex>
class VertexBuffer {
	
public:
	
	size_t capacity() const { return _capacity; }
	
	virtual void setData(const Vertex * vertices, size_t count, size_t offset = 0, BufferFlags flags = 0) = 0;
	
	virtual Vertex * lock(BufferFlags flags = 0) = 0;
	virtual void unlock() = 0;
	
	virtual void draw(Renderer::Primitive primitive, size_t count, size_t offset = 0) const = 0;
	virtual void drawIndexed(Renderer::Primitive primitive, size_t count, size_t offset, unsigned short * indices, size_t nbindices) const = 0;
	
	virtual ~VertexBuffer() { };
	
protected:
	
	VertexBuffer(size_t capacity) : _capacity(capacity) { }
	
	const size_t _capacity;
	
};

template <class Vertex>
class CircularVertexBuffer {
	
public:
	
	VertexBuffer<Vertex> * const vb;
	
	size_t pos;
	
	CircularVertexBuffer(VertexBuffer<Vertex> * _vb) : vb(_vb), pos(0) { }
	
	void draw(Renderer::Primitive primitive, const Vertex * vertices, size_t count) {
		
		const Vertex * src = vertices;
		
		size_t num = std::min(count, vb->capacity());
		
		// Make sure num is a multiple of the primitive's stride.
		if(primitive == Renderer::TriangleList) {
			arx_assert(count % 3 == 0);
			num -= num % 3;
		} else if(primitive == Renderer::LineList) {
			arx_assert((count & 1) == 0);
			num &= ~1;
		} else if(primitive == Renderer::TriangleStrip && num != count) {
			// Draw an even number of triangles so we don't flip front and back faces between draw calls.
			num &= 1;
		}
		
		size_t dst_offset = (pos + num > vb->capacity()) ? 0 : pos;
		
		vb->setData(src, num, dst_offset, dst_offset ? NoOverwrite : DiscardContents);
		vb->draw(primitive, num, dst_offset);
		
		pos = dst_offset + num;
		src += num, count -= num;
		
		//if(!count) {
			return;
		//}
		
		switch(primitive) {
			
			case Renderer::TriangleList: do {
				num = std::min(count, vb->capacity());
				num -= num % 3;
				vb->setData(src, num, 0, DiscardContents);
				vb->draw(primitive, num);
				src += num, count -= num, pos = num;
			} while(count); break;
			
			case Renderer::TriangleStrip: do {
				count += 2, src -= 2;
				num = std::min(count, vb->capacity());
				if(num != count) {
					// Draw an even number of triangles so we don't flip front and back faces between draw calls.
					num -= num & 1;
				}
				vb->setData(src, num, 0, DiscardContents);
				vb->draw(primitive, num);
				src += num, count -= num, pos = num;
			} while(count); break;
			
			case Renderer::TriangleFan: do {
				count += 1, src -= 1;
				num = std::min(count, vb->capacity() - 1);
				vb->setData(vertices, 1, 0, DiscardContents);
				vb->setData(src, num, 1, NoOverwrite);
				vb->draw(primitive, num + 1);
				src += num, count -= num, pos = num + 1;
			} while(count); break;
			
			case Renderer::LineList: do {
				num = std::min(count, vb->capacity()) & ~1;
				vb->setData(src, num, 0, DiscardContents);
				vb->draw(primitive, num);
				src += num, count -= num, pos = num;
				break;
			} while(count); break;
			
			case Renderer::LineStrip: do {
				count += 1, src -= 1;
				num = std::min(count, vb->capacity());
				vb->setData(src, num, 0, DiscardContents);
				vb->draw(primitive, num);
				src += num, count -= num, pos = num;
				break;
			} while(count); break;
			
			default:
				arx_assert_msg(false, "too large vertex array (%d) for primitive %d", count + num, primitive);
			
		}
		
	}
	
	~CircularVertexBuffer() {
		delete vb;
	}
	
};

#endif // ARX_GRAPHICS_VERTEXBUFFER_H;
