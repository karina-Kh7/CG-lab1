#include "application.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace application {
    namespace {

        // 1. Простая математика для матриц 4x4
        struct Mat4 {
            float m[16]{};
        };
        // Создаёт единичную матрицу 4x4 — основу для остальных преобразований.
        Mat4 identity() {
            Mat4 result{};
            for (int i = 0; i < 4; ++i)
                result.m[i * 5] = 1.0f;
            return result;
        }
        // Перемножает две матрицы 4x4, чтобы объединять несколько преобразований в одно.
        Mat4 multiply(const Mat4& a, const Mat4& b) {
            Mat4 result{};
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    for (int k = 0; k < 4; ++k)
                        result.m[column * 4 + row] +=
                        a.m[k * 4 + row] * b.m[column * 4 + k];
            return result;
        }
        // Создаёт матрицу переноса объекта на заданное смещение по X, Y и Z.
        Mat4 translate(float x, float y, float z) {
            Mat4 result = identity();
            result.m[12] = x;
            result.m[13] = y;
            result.m[14] = z;
            return result;
        }
        // Создаёт матрицу масштабирования по трём осям.
        Mat4 scale(float x, float y, float z) {
            Mat4 result = identity();
            result.m[0] = x;
            result.m[5] = y;
            result.m[10] = z;
            return result;
        }
        // Создаёт матрицу поворота вокруг оси X.
        Mat4 rotateX(float angle) {
            Mat4 result = identity();
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            result.m[5] = c;
            result.m[6] = s;
            result.m[9] = -s;
            result.m[10] = c;
            return result;
        }
        // Создаёт матрицу поворота вокруг оси Y.
        Mat4 rotateY(float angle) {
            Mat4 result = identity();
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            result.m[0] = c;
            result.m[2] = -s;
            result.m[8] = s;
            result.m[10] = c;
            return result;
        }
        // Создаёт матрицу поворота вокруг оси Z.
        Mat4 rotateZ(float angle) {
            Mat4 result = identity();
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            result.m[0] = c;
            result.m[1] = s;
            result.m[4] = -s;
            result.m[5] = c;
            return result;
        }
        constexpr float pi = 3.14159265358979323846f;
        // Переводит угол из градусов в радианы, так как sin и cos работают с радианами.
        float radians(float degrees) {
            return degrees * pi / 180.0f;
        }
        // Создаёт матрицу перспективной проекции: дальние части объекта выглядят меньше.
        Mat4 perspective(float aspect) {
            constexpr float nearPlane = 0.1f;
            constexpr float farPlane = 100.0f;
            const float f = 1.0f / std::tan(radians(55.0f) / 2.0f);
            Mat4 result{};
            result.m[0] = f / aspect;
            result.m[5] = f;
            result.m[10] = farPlane / (nearPlane - farPlane);
            result.m[11] = -1.0f;
            result.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
            return result;
        }
        // Создаёт ортографическую проекцию: размер объекта не зависит от расстояния до камеры.
        Mat4 orthographic(float aspect) {
            const float height = 4.0f;
            const float width = height * aspect;
            const float nearPlane = 0.1f;
            const float farPlane = 100.0f;
            Mat4 result = identity();
            result.m[0] = 2.0f / width;
            result.m[5] = 2.0f / height;
            result.m[10] = 1.0f / (nearPlane - farPlane);
            result.m[14] = nearPlane / (nearPlane - farPlane);
            return result;
        }

        // 2. Геометрия правильного икосаэдра
        // Одна вершина хранит своё положение в 3D и RGB-цвет.
        struct Vertex {
            float position[3];
            float color[3];
        };
        // Золотое сечение используется в стандартных координатах правильного икосаэдра.
        constexpr float phi = 1.61803398875f;
        // Формируем 12 вершин икосаэдра и процедурно вычисляем цвет каждой вершины.
        const std::array<Vertex, 12> vertices = [] {
            const float positions[12][3] = {
                {0, 1, phi}, {0, -1, phi}, {0, 1, -phi}, {0, -1, -phi},
                {1, phi, 0}, {-1, phi, 0}, {1, -phi, 0}, {-1, -phi, 0},
                {phi, 0, 1}, {phi, 0, -1}, {-phi, 0, 1}, {-phi, 0, -1}
            };
            std::array<Vertex, 12> result{};
            for (int i = 0; i < 12; ++i) {
                const float x = positions[i][0];
                const float y = positions[i][1];
                const float z = positions[i][2];
                result[i].position[0] = x;
                result[i].position[1] = y;
                result[i].position[2] = z;
                // Доп. №5: цвет вершины зависит от её локальных координат.
                result[i].color[0] = 0.25f + 0.75f * (x + phi) / (2.0f * phi);
                result[i].color[1] = 0.25f + 0.75f * (y + phi) / (2.0f * phi);
                result[i].color[2] = 0.25f + 0.75f * (z + phi) / (2.0f * phi);
            }
            return result;
            }();
        // 20 треугольных граней = 60 индексов.
        constexpr uint16_t indices[] = {
            0,1,8,   0,10,1,  0,4,5,   0,8,4,   0,5,10,
            1,10,7,  1,7,6,   1,6,8,
            2,3,11,  2,9,3,   2,5,4,   2,11,5,  2,4,9,
            3,6,7,   3,9,6,   3,7,11,
            4,8,9,   5,11,10, 6,9,8,   7,10,11
        };

        // 3. Vulkan-данные
        // Данные, которые каждый кадр передаются в vertex shader через uniform buffer.
        struct alignas(16) Uniform {
            Mat4 mvp;
            float tint[4];
        };
        // Упрощённая обёртка над Vulkan-буфером и выделенной для него памятью.
        struct Buffer {
            VkBuffer handle = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
        };
        Buffer vertexBuffer;
        Buffer indexBuffer;
        Buffer uniformBuffer;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

        // 4. Параметры фигуры и анимации
        float position[3] = { 0.0f, 0.0f, 0.0f };
        float rotation[3] = { 18.0f, 25.0f, 0.0f };
        float objectScale[3] = { 1.0f, 1.0f, 1.0f };
        float colorMultiplier[3] = { 1.0f, 1.0f, 1.0f };
        float orbitRadius = 1.1f;
        float animationSpeed = 1.0f;
        float animationPhase = 0.0f;
        bool animationPaused = false;
        bool usePerspective = true;
        double previousTime = -1.0;

        // 5. Работа с буферами
        // Создаёт Vulkan-буфер нужного размера и выделяет для него память через VMA.
        bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& target) {
            auto& context = graphics::internal::context;
            const VkBufferCreateInfo bufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            const VmaAllocationCreateInfo allocationInfo{
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };
            return vmaCreateBuffer(
                context.allocator, &bufferInfo, &allocationInfo,
                &target.handle, &target.allocation, nullptr
            ) == VK_SUCCESS;
        }
        // Освобождает Vulkan-буфер и связанную с ним память.
        void destroyBuffer(Buffer& buffer) {
            if (buffer.handle != VK_NULL_HANDLE) {
                vmaDestroyBuffer(
                    graphics::internal::context.allocator,
                    buffer.handle,
                    buffer.allocation
                );
            }
            buffer = {};
        }
        // Копирует данные из обычной памяти CPU в память Vulkan-буфера.
        bool writeBuffer(const Buffer& buffer, const void* data, size_t size) {
            auto allocator = graphics::internal::context.allocator;
            void* mappedMemory = nullptr;
            if (vmaMapMemory(allocator, buffer.allocation, &mappedMemory) != VK_SUCCESS)
                return false;
            std::memcpy(mappedMemory, data, size);
            vmaFlushAllocation(allocator, buffer.allocation, 0, size);
            vmaUnmapMemory(allocator, buffer.allocation);
            return true;
        }

        // 6. Загрузка шейдеров
        // Загружает с диска уже скомпилированный SPIR-V shader-файл.
        std::vector<char> loadShaderFile(const char* path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                throw std::runtime_error(std::string("Cannot open shader: ") + path);
            const auto fileSize = file.tellg();
            if (fileSize <= 0 || fileSize % 4 != 0)
                throw std::runtime_error("Invalid SPIR-V file");
            std::vector<char> bytes(static_cast<size_t>(fileSize));
            file.seekg(0);
            file.read(bytes.data(), fileSize);
            if (!file)
                throw std::runtime_error("Cannot read SPIR-V file");
            return bytes;
        }
        // Создаёт Vulkan shader module из загруженного SPIR-V-кода.
        VkShaderModule createShader(const char* path) {
            auto& context = graphics::internal::context;
            const auto code = loadShaderFile(path);
            const VkShaderModuleCreateInfo shaderInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(code.data())
            };
            VkShaderModule shaderModule = VK_NULL_HANDLE;
            if (vkCreateShaderModule(context.device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS)
                throw std::runtime_error(std::string("Cannot create shader: ") + path);
            return shaderModule;
        }

        // 7. Graphics pipeline
        // Настраивает graphics pipeline: шейдеры, вершины, треугольники, depth test и другие этапы отрисовки.
        bool createPipeline() {
            auto& context = graphics::internal::context;
            VkShaderModule vertexShader = VK_NULL_HANDLE;
            VkShaderModule fragmentShader = VK_NULL_HANDLE;
            try {
                vertexShader = createShader("shaders/icosahedron.vert.spv");
                fragmentShader = createShader("shaders/icosahedron.frag.spv");
            }
            catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                if (vertexShader)
                    vkDestroyShaderModule(context.device, vertexShader, nullptr);
                return false;
            }
            const VkPipelineShaderStageCreateInfo shaderStages[] = {
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader,
                    .pName = "main"
                },
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader,
                    .pName = "main"
                }
            };
            // Описываем, как GPU должен читать одну структуру Vertex из vertex buffer.
            const VkVertexInputBindingDescription vertexBinding{
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            };
            // Связываем position и color из Vertex с location 0 и 1 в vertex shader.
            const VkVertexInputAttributeDescription vertexAttributes[] = {
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position))},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, color))}
            };
            const VkPipelineVertexInputStateCreateInfo vertexInput{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexBinding,
                .vertexAttributeDescriptionCount = 2,
                .pVertexAttributeDescriptions = vertexAttributes
            };
            // Каждые три индекса образуют отдельный треугольник.
            const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
            };
            const VkPipelineViewportStateCreateInfo viewportState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1
            };
            // Растеризатор превращает треугольники в набор фрагментов для последующей закраски.
            const VkPipelineRasterizationStateCreateInfo rasterizer{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f
            };
            const VkPipelineMultisampleStateCreateInfo multisampling{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
            };
            // Depth test оставляет видимыми ближайшие к камере поверхности.
            const VkPipelineDepthStencilStateCreateInfo depthStencil{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_LESS
            };
            const VkPipelineColorBlendAttachmentState blendAttachment{
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            };
            const VkPipelineColorBlendStateCreateInfo colorBlending{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &blendAttachment
            };
            const VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };
            const VkPipelineDynamicStateCreateInfo dynamicState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = dynamicStates
            };
            const VkGraphicsPipelineCreateInfo pipelineInfo{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shaderStages,
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = context.render_pass,
                .subpass = 0
            };
            const VkResult result = vkCreateGraphicsPipelines(
                context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline
            );
            vkDestroyShaderModule(context.device, fragmentShader, nullptr);
            vkDestroyShaderModule(context.device, vertexShader, nullptr);
            return result == VK_SUCCESS;
        }
    } // namespace

    // 8. Инициализация

    // Создаёт все ресурсы приложения перед началом отрисовки.
    bool initialize() {
        auto& context = graphics::internal::context;
        if (!createBuffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer) ||
            !createBuffer(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer) ||
            !createBuffer(sizeof(Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffer) ||
            !writeBuffer(vertexBuffer, vertices.data(), sizeof(vertices)) ||
            !writeBuffer(indexBuffer, indices, sizeof(indices))) {
            shutdown();
            return false;
        }
        // Описываем uniform buffer, который vertex shader будет получать через binding 0.
        const VkDescriptorSetLayoutBinding layoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };
        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &layoutBinding
        };
        if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            return (shutdown(), false);
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout
        };
        if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            return (shutdown(), false);
        const VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1
        };
        // Создаём pool, из которого будет выделен один descriptor set.
        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };
        if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            return (shutdown(), false);
        const VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout
        };
        if (vkAllocateDescriptorSets(context.device, &allocateInfo, &descriptorSet) != VK_SUCCESS)
            return (shutdown(), false);
        const VkDescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffer.handle,
            .offset = 0,
            .range = sizeof(Uniform)
        };
        // Связываем descriptor set с нашим uniform buffer.
        const VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo
        };
        vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
        if (!createPipeline())
            return (shutdown(), false);
        return true;
    }

    // 9. Освобождение ресурсов

    // Корректно освобождает созданные Vulkan-ресурсы при завершении программы.
    void shutdown() {
        auto& context = graphics::internal::context;
        vkQueueWaitIdle(context.graphics_queue);
        if (pipeline)
            vkDestroyPipeline(context.device, pipeline, nullptr);
        if (pipelineLayout)
            vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        if (descriptorPool)
            vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
        if (descriptorSetLayout)
            vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
        destroyBuffer(uniformBuffer);
        destroyBuffer(indexBuffer);
        destroyBuffer(vertexBuffer);
    }

    // 10. Интерфейс и анимация

    // Обновляет время анимации и строит панель управления ImGui.
    void update(double time) {
        if (previousTime < 0.0)
            previousTime = time;
        const double deltaTime = std::clamp(time - previousTime, 0.0, 0.1);
        previousTime = time;
        if (!animationPaused)
            animationPhase += animationSpeed * static_cast<float>(deltaTime);
        ImGui::Begin("Lab 1: icosahedron (variant 11)");
        // Доп. №1
        ImGui::Checkbox("Perspective projection", &usePerspective);
        // Доп. №2
        ImGui::DragFloat3("Position", position, 0.02f);
        ImGui::DragFloat3("Rotation (degrees)", rotation, 0.5f);
        ImGui::DragFloat3("Scale", objectScale, 0.01f, 0.1f, 4.0f);
        // Доп. №4
        ImGui::ColorEdit3("Color multiplier", colorMultiplier);
        ImGui::Separator();
        // Доп. №3
        ImGui::Checkbox("Pause animation", &animationPaused);
        ImGui::SliderFloat("Orbit speed", &animationSpeed, 0.0f, 4.0f);
        ImGui::SliderFloat("Orbit radius", &orbitRadius, 0.0f, 2.0f);
        ImGui::TextUnformatted("Vertex colors depend on local XYZ coordinates.");
        ImGui::End();
    }

    // 11. Отрисовка

    // Формирует матрицы текущего кадра и записывает команды отрисовки икосаэдра.
    void render(const graphics::internal::FrameData& fd) {
        auto& context = graphics::internal::context;
        if (!fd.command_buffer || !fd.framebuffer)
            return;
        // Отношение ширины окна к высоте нужно для правильной формы проекции.
        const float aspect =
            static_cast<float>(context.swapchain_extent.width) /
            static_cast<float>(context.swapchain_extent.height);
        // Доп. №1: выбираем тип проекции.
        const Mat4 projection = usePerspective
            ? perspective(aspect)
            : orthographic(aspect);
        // Матрица вида отодвигает сцену от камеры, чтобы объект оказался в поле зрения.
        const Mat4 view = translate(0.0f, 0.0f, -8.0f);
        // Доп. №3: движение по сложной траектории.
        const Mat4 orbit = translate(
            orbitRadius * std::cos(animationPhase),
            0.35f * std::sin(2.0f * animationPhase),
            orbitRadius * std::sin(animationPhase)
        );
        const Mat4 userTranslation = translate(position[0], position[1], position[2]);
        const Mat4 rotationZ = rotateZ(radians(rotation[2] + animationPhase * 30.0f));
        const Mat4 rotationY = rotateY(radians(rotation[1] + animationPhase * 50.0f));
        const Mat4 rotationX = rotateX(radians(rotation[0]));
        const Mat4 scaling = scale(objectScale[0], objectScale[1], objectScale[2]);
        // Model-матрица объединяет масштабирование, поворот, пользовательский перенос и движение по траектории.
        const Mat4 model = multiply(
            multiply(userTranslation, orbit),
            multiply(
                multiply(rotationZ, rotationY),
                multiply(rotationX, scaling)
            )
        );
        // Итоговая MVP-матрица и выбранный цвет передаются в shader через uniform buffer.
        Uniform uniformData{
            multiply(projection, multiply(view, model)),
            {colorMultiplier[0], colorMultiplier[1], colorMultiplier[2], 1.0f}
        };
        if (!writeBuffer(uniformBuffer, &uniformData, sizeof(uniformData)))
            std::cerr << "Uniform update failed\n";
        // Дальше идёт обычная запись команд Vulkan.
        // Начинаем заново записывать команды для текущего кадра.
        vkResetCommandBuffer(fd.command_buffer, 0);
        const VkCommandBufferBeginInfo commandBufferBegin{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        vkBeginCommandBuffer(fd.command_buffer, &commandBufferBegin);
        const VkClearValue clearValues[] = {
            {.color = {{0.07f, 0.09f, 0.13f, 1.0f}}},
            {.depthStencil = {1.0f, 0}}
        };
        const VkRenderPassBeginInfo renderPassBegin{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = context.render_pass,
            .framebuffer = fd.framebuffer,
            .renderArea = {{0, 0}, context.swapchain_extent},
            .clearValueCount = 2,
            .pClearValues = clearValues
        };
        // Начинаем render pass и выбираем созданный graphics pipeline.
        vkCmdBeginRenderPass(fd.command_buffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        // Отрицательная высота переворачивает ось Y для Vulkan.
        const VkViewport viewport{
            0.0f,
            static_cast<float>(context.swapchain_extent.height),
            static_cast<float>(context.swapchain_extent.width),
            -static_cast<float>(context.swapchain_extent.height),
            0.0f,
            1.0f
        };
        const VkRect2D scissor{ {0, 0}, context.swapchain_extent };
        vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);
        const VkDeviceSize vertexOffset = 0;
        // Подключаем буферы геометрии: координаты/цвета вершин и индексы треугольников.
        vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertexBuffer.handle, &vertexOffset);
        vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT16);
        // Подключаем descriptor set, через который shader получает MVP-матрицу и цвет.
        vkCmdBindDescriptorSets(
            fd.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &descriptorSet,
            0,
            nullptr
        );
        // Запускаем отрисовку 60 индексов, то есть 20 треугольных граней икосаэдра.
        vkCmdDrawIndexed(fd.command_buffer, 60, 1, 0, 0, 0);
        vkCmdEndRenderPass(fd.command_buffer);
        vkEndCommandBuffer(fd.command_buffer);
    }
} // namespace application