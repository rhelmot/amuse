namespace boo {
class D3D11TextureR;
class D3D11TextureCubeR;
struct D3D11CommandQueue; /* final : IGraphicsCommandQueue {
  Platform platform() const override;
  const char* platformName() const override;
  D3D11Context* m_ctx;
  D3D11Context::Window* m_windowCtx;
  IGraphicsContext* m_parent;
  ComPtr<ID3D11DeviceContext1> m_deferredCtx;
  ComPtr<ID3DUserDefinedAnnotation> m_deferredAnnot;

  size_t m_fillBuf;
  size_t m_completeBuf;
  size_t m_drawBuf;
  bool m_running;

  std::mutex m_mt;
  std::condition_variable m_cv;
  std::mutex m_initmt;
  std::condition_variable m_initcv;
  std::unique_lock<std::mutex> m_initlk;
  std::thread m_thr;

  struct CommandList {
    ComPtr<ID3D11CommandList> list;
    std::vector<boo::ObjToken<boo::IObj>> resTokens;
    boo::ObjToken<ITextureR> workDoPresent;

    void reset();
  };
  std::array<CommandList, 3> m_cmdLists;

  std::recursive_mutex m_dynamicLock;
  void ProcessDynamicLoads(ID3D11DeviceContext* ctx);
  static void RenderingWorker(D3D11CommandQueue* self);

  D3D11CommandQueue(D3D11Context* ctx, D3D11Context::Window* windowCtx, IGraphicsContext* parent);

  void startRenderer() override;

  void stopRenderer() override;

  ~D3D11CommandQueue() override;

  void setShaderDataBinding(const boo::ObjToken<IShaderDataBinding>& binding) override;

  boo::ObjToken<ITexture> m_boundTarget;
  void setRenderTarget(const boo::ObjToken<ITextureR>& target) override;

  int m_boundFace;
  void setRenderTarget(const ObjToken<ITextureCubeR>& target, int face) override;

  void setViewport(const SWindowRect& rect, float znear, float zfar) override;

  void setScissor(const SWindowRect& rect) override;

  std::unordered_map<D3D11TextureR*, std::pair<size_t, size_t>> m_texResizes;
  void resizeRenderTexture(const boo::ObjToken<ITextureR>& tex, size_t width, size_t height) override;

  std::unordered_map<D3D11TextureCubeR*, std::pair<size_t, size_t>> m_cubeTexResizes;
  void resizeRenderTexture(const boo::ObjToken<ITextureCubeR>& tex, size_t width, size_t mips) override;

  void generateMipmaps(const ObjToken<ITextureCubeR>& tex) override;

  void schedulePostFrameHandler(std::function<void()>&& func) override;

  std::array<float, 4> m_clearColor;
  void setClearColor(const float rgba[4]) override;

  void clearTarget(bool render = true, bool depth = true) override;

  void draw(size_t start, size_t count) override;

  void drawIndexed(size_t start, size_t count, size_t baseVertex) override;

  void drawInstances(size_t start, size_t count, size_t instCount, size_t startInst) override;

  void drawInstancesIndexed(size_t start, size_t count, size_t instCount, size_t startInst) override;

  void _resolveBindTexture(ID3D11DeviceContext1* ctx, const D3D11TextureR* tex, const SWindowRect& rect, bool tlOrigin,
                           int bindIdx, bool color, bool depth);

  void resolveBindTexture(const boo::ObjToken<ITextureR>& texture, const SWindowRect& rect, bool tlOrigin, int bindIdx,
                          bool color, bool depth, bool clearDepth) override;

  boo::ObjToken<ITextureR> m_doPresent;
  void resolveDisplay(const boo::ObjToken<ITextureR>& source) override;

  void execute() override;

#ifdef BOO_GRAPHICS_DEBUG_GROUPS
  void pushDebugGroup(const char* name, const std::array<float, 4>& color) override;

  void popDebugGroup() override;
#endif
}; */
}





// BLAH

struct CD3D11_DEPTH_STENCIL_VIEW_DESC : public D3D11_DEPTH_STENCIL_VIEW_DESC
{
	CD3D11_DEPTH_STENCIL_VIEW_DESC()
	{}
	explicit CD3D11_DEPTH_STENCIL_VIEW_DESC( const D3D11_DEPTH_STENCIL_VIEW_DESC& o ) :
		D3D11_DEPTH_STENCIL_VIEW_DESC( o )
	{}
	explicit CD3D11_DEPTH_STENCIL_VIEW_DESC(
			D3D11_DSV_DIMENSION viewDimension,
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
			UINT mipSlice = 0,
			UINT firstArraySlice = 0,
			UINT arraySize = -1,
			UINT flags = 0 )
	{
		Format = format;
		ViewDimension = viewDimension;
		Flags = flags;
		switch (viewDimension)
		{
			case D3D11_DSV_DIMENSION_TEXTURE1D:
				Texture1D.MipSlice = mipSlice;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
				Texture1DArray.MipSlice = mipSlice;
				Texture1DArray.FirstArraySlice = firstArraySlice;
				Texture1DArray.ArraySize = arraySize;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2D:
				Texture2D.MipSlice = mipSlice;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
				Texture2DArray.MipSlice = mipSlice;
				Texture2DArray.FirstArraySlice = firstArraySlice;
				Texture2DArray.ArraySize = arraySize;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2DMS:
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
				Texture2DMSArray.FirstArraySlice = firstArraySlice;
				Texture2DMSArray.ArraySize = arraySize;
				break;
			default: break;
		}
	}
	explicit CD3D11_DEPTH_STENCIL_VIEW_DESC(
			_In_ ID3D11Texture1D* pTex1D,
			D3D11_DSV_DIMENSION viewDimension,
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
			UINT mipSlice = 0,
			UINT firstArraySlice = 0,
			UINT arraySize = -1,
			UINT flags = 0 )
	{
		ViewDimension = viewDimension;
		Flags = flags;
		if (DXGI_FORMAT_UNKNOWN == format ||
				(-1 == arraySize && D3D11_DSV_DIMENSION_TEXTURE1DARRAY == viewDimension))
		{
			D3D11_TEXTURE1D_DESC TexDesc;
			pTex1D->GetDesc( &TexDesc );
			if (DXGI_FORMAT_UNKNOWN == format) format = TexDesc.Format;
			if (-1 == arraySize) arraySize = TexDesc.ArraySize - firstArraySlice;
		}
		Format = format;
		switch (viewDimension)
		{
			case D3D11_DSV_DIMENSION_TEXTURE1D:
				Texture1D.MipSlice = mipSlice;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
				Texture1DArray.MipSlice = mipSlice;
				Texture1DArray.FirstArraySlice = firstArraySlice;
				Texture1DArray.ArraySize = arraySize;
				break;
			default: break;
		}
	}
	explicit CD3D11_DEPTH_STENCIL_VIEW_DESC(
			_In_ ID3D11Texture2D* pTex2D,
			D3D11_DSV_DIMENSION viewDimension,
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
			UINT mipSlice = 0,
			UINT firstArraySlice = 0,
			UINT arraySize = -1,
			UINT flags = 0 )
	{
		ViewDimension = viewDimension;
		Flags = flags;
		if (DXGI_FORMAT_UNKNOWN == format || 
				(-1 == arraySize &&
				 (D3D11_DSV_DIMENSION_TEXTURE2DARRAY == viewDimension ||
				  D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY == viewDimension)))
		{
			D3D11_TEXTURE2D_DESC TexDesc;
			pTex2D->GetDesc( &TexDesc );
			if (DXGI_FORMAT_UNKNOWN == format) format = TexDesc.Format;
			if (-1 == arraySize) arraySize = TexDesc.ArraySize - firstArraySlice;
		}
		Format = format;
		switch (viewDimension)
		{
			case D3D11_DSV_DIMENSION_TEXTURE2D:
				Texture2D.MipSlice = mipSlice;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
				Texture2DArray.MipSlice = mipSlice;
				Texture2DArray.FirstArraySlice = firstArraySlice;
				Texture2DArray.ArraySize = arraySize;
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2DMS:
				break;
			case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
				Texture2DMSArray.FirstArraySlice = firstArraySlice;
				Texture2DMSArray.ArraySize = arraySize;
				break;
			default: break;
		}
	}
	~CD3D11_DEPTH_STENCIL_VIEW_DESC() {}
	operator const D3D11_DEPTH_STENCIL_VIEW_DESC&() const { return *this; }
};
