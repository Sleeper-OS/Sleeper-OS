/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2020 Mellanox Technologies Ltd. */

#ifndef __MLX5_VDPA_H__
#define __MLX5_VDPA_H__

#include <linux/etherdevice.h>
#include <linux/vringh.h>
#include <linux/vdpa.h>
#include <linux/mlx5/driver.h>

#define MLX5V_ETH_HARD_MTU (ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN)

struct mlx5_vdpa_direct_mr {
	u64 start;
	u64 end;
	u32 perm;
	u32 mr;
	struct sg_table sg_head;
	int log_size;
	int nsg;
	int nent;
	struct list_head list;
	u64 offset;
};

struct mlx5_vdpa_mr {
	u32 mkey;

	/* list of direct MRs descendants of this indirect mr */
	struct list_head head;
	unsigned long num_directs;
	unsigned long num_klms;

	struct vhost_iotlb *iotlb;

	bool user_mr;

	refcount_t refcount;
	struct list_head mr_list;
};

struct mlx5_vdpa_resources {
	u32 pdn;
	struct mlx5_uars_page *uar;
	void __iomem *kick_addr;
	u64 phys_kick_addr;
	u16 uid;
	u32 null_mkey;
	bool valid;
};

struct mlx5_control_vq {
	struct vhost_iotlb *iotlb;
	/* spinlock to synchronize iommu table */
	spinlock_t iommu_lock;
	struct vringh vring;
	bool ready;
	u64 desc_addr;
	u64 device_addr;
	u64 driver_addr;
	struct vdpa_callback event_cb;
	struct vringh_kiov riov;
	struct vringh_kiov wiov;
	unsigned short head;
	unsigned int received_desc;
	unsigned int completed_desc;
};

struct mlx5_vdpa_wq_ent {
	struct work_struct work;
	struct mlx5_vdpa_dev *mvdev;
};

enum {
	MLX5_VDPA_DATAVQ_GROUP,
	MLX5_VDPA_CVQ_GROUP,
	MLX5_VDPA_DATAVQ_DESC_GROUP,
	MLX5_VDPA_NUMVQ_GROUPS
};

enum {
	MLX5_VDPA_NUM_AS = 2
};

struct mlx5_vdpa_mr_resources {
	struct mlx5_vdpa_mr *mr[MLX5_VDPA_NUM_AS];
	unsigned int group2asid[MLX5_VDPA_NUMVQ_GROUPS];

	/* Pre-deletion mr list */
	struct list_head mr_list_head;

	/* Deferred mr list */
	struct list_head mr_gc_list_head;
	struct workqueue_struct *wq_gc;
	struct delayed_work gc_dwork_ent;

	struct mutex lock;

	atomic_t shutdown;
};

struct mlx5_vdpa_dev {
	struct vdpa_device vdev;
	struct mlx5_core_dev *mdev;
	struct mlx5_vdpa_resources res;
	struct mlx5_vdpa_mr_resources mres;

	u64 mlx_features;
	u64 actual_features;
	u8 status;
	u32 max_vqs;
	u16 max_idx;
	u32 generation;

	struct mlx5_control_vq cvq;
	struct workqueue_struct *wq;
	bool suspended;

	struct mlx5_async_ctx async_ctx;
};

struct mlx5_vdpa_async_cmd {
	int err;
	struct mlx5_async_work cb_work;
	struct completion cmd_done;

	void *in;
	size_t inlen;

	void *out;
	size_t outlen;
};

int mlx5_vdpa_create_tis(struct mlx5_vdpa_dev *mvdev, void *in, u32 *tisn);
void mlx5_vdpa_destroy_tis(struct mlx5_vdpa_dev *mvdev, u32 tisn);
int mlx5_vdpa_create_rqt(struct mlx5_vdpa_dev *mvdev, void *in, int inlen, u32 *rqtn);
int mlx5_vdpa_modify_rqt(struct mlx5_vdpa_dev *mvdev, void *in, int inlen, u32 rqtn);
void mlx5_vdpa_destroy_rqt(struct mlx5_vdpa_dev *mvdev, u32 rqtn);
int mlx5_vdpa_create_tir(struct mlx5_vdpa_dev *mvdev, void *in, u32 *tirn);
void mlx5_vdpa_destroy_tir(struct mlx5_vdpa_dev *mvdev, u32 tirn);
int mlx5_vdpa_alloc_transport_domain(struct mlx5_vdpa_dev *mvdev, u32 *tdn);
void mlx5_vdpa_dealloc_transport_domain(struct mlx5_vdpa_dev *mvdev, u32 tdn);
int mlx5_vdpa_alloc_resources(struct mlx5_vdpa_dev *mvdev);
void mlx5_vdpa_free_resources(struct mlx5_vdpa_dev *mvdev);
int mlx5_vdpa_create_mkey(struct mlx5_vdpa_dev *mvdev, u32 *mkey, u32 *in,
			  int inlen);
int mlx5_vdpa_destroy_mkey(struct mlx5_vdpa_dev *mvdev, u32 mkey);
struct mlx5_vdpa_mr *mlx5_vdpa_create_mr(struct mlx5_vdpa_dev *mvdev,
					 struct vhost_iotlb *iotlb);
int mlx5_vdpa_init_mr_resources(struct mlx5_vdpa_dev *mvdev);
void mlx5_vdpa_destroy_mr_resources(struct mlx5_vdpa_dev *mvdev);
void mlx5_vdpa_clean_mrs(struct mlx5_vdpa_dev *mvdev);
void mlx5_vdpa_get_mr(struct mlx5_vdpa_dev *mvdev,
		      struct mlx5_vdpa_mr *mr);
void mlx5_vdpa_put_mr(struct mlx5_vdpa_dev *mvdev,
		      struct mlx5_vdpa_mr *mr);
void mlx5_vdpa_update_mr(struct mlx5_vdpa_dev *mvdev,
			 struct mlx5_vdpa_mr *mr,
			 unsigned int asid);
int mlx5_vdpa_update_cvq_iotlb(struct mlx5_vdpa_dev *mvdev,
				struct vhost_iotlb *iotlb,
				unsigned int asid);
int mlx5_vdpa_create_dma_mr(struct mlx5_vdpa_dev *mvdev);
int mlx5_vdpa_reset_mr(struct mlx5_vdpa_dev *mvdev, unsigned int asid);
int mlx5_vdpa_exec_async_cmds(struct mlx5_vdpa_dev *mvdev,
			      struct mlx5_vdpa_async_cmd *cmds,
			      int num_cmds);

#define mlx5_vdpa_err(__dev, format, ...)                                                          \
	dev_err((__dev)->mdev->device, "%s:%d:(pid %d) error: " format, __func__, __LINE__,        \
		 current->pid, ##__VA_ARGS__)


#define mlx5_vdpa_warn(__dev, format, ...)                                                         \
	dev_warn((__dev)->mdev->device, "%s:%d:(pid %d) warning: " format, __func__, __LINE__,     \
		 current->pid, ##__VA_ARGS__)

#define mlx5_vdpa_info(__dev, format, ...)                                                         \
	dev_info((__dev)->mdev->device, "%s:%d:(pid %d): " format, __func__, __LINE__,             \
		 current->pid, ##__VA_ARGS__)

#define mlx5_vdpa_dbg(__dev, format, ...)                                                          \
	dev_debug((__dev)->mdev->device, "%s:%d:(pid %d): " format, __func__, __LINE__,            \
		  current->pid, ##__VA_ARGS__)

#endif /* __MLX5_VDPA_H__ */